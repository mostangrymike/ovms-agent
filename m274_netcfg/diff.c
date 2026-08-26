/*
 * DIFF.C - Phase 2 configuration diff implementation
 * DEC C / OpenVMS C89 compatible
 *
 * Implements cfgdiff_compare_paths as declared in DIFF.H
 * Uses FILEUTIL to read files into line arrays, and emits an optional
 * compact unified-diff-like output with modest lookahead alignment.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "diff.h"
#include "fileutil.h"

/* Modest lookahead window (lines) for alignment */
#ifndef CDIFF_LOOKAHEAD
#define CDIFF_LOOKAHEAD 16
#endif

/* Hunk entry for unified output */
typedef struct hentry {
    char tag;          /* ' ' context, '-' removal, '+' addition */
    const char *text;  /* pointer to source line (no trailing newline) */
} HENTRY;

/* Dynamic hunk buffer */
typedef struct hbuf {
    HENTRY *v;
    unsigned long n;
    unsigned long cap;
} HBUF;

/* Ring buffer for recent context lines prior to a change */
typedef struct prebuf {
    const char **v;            /* pointers to recent equal lines */
    unsigned int cap;          /* capacity equals configured context */
    unsigned int count;        /* number of valid entries (<= cap) */
    unsigned int start;        /* index of oldest entry within v */
} PREBUF;

/* Local helpers (static to avoid external symbol exposure) */
static void hb_init(HBUF *hb);
static int  hb_ensure(HBUF *hb, unsigned long need);
static void hb_clear(HBUF *hb);
static void hb_free(HBUF *hb);
static int  hb_push(HBUF *hb, char tag, const char *txt);

static void pb_init(PREBUF *pb, unsigned int cap);
static void pb_free(PREBUF *pb);
static void pb_push(PREBUF *pb, const char *txt);
static unsigned int pb_take_last(PREBUF *pb, unsigned int k, const char ***outp);

static int  write_udiff_header(FILE *f, const char *oldp, int old_absent, const char *newp);
static int  write_udiff_hunk(FILE *f,
                             unsigned long ostart, unsigned long ocnt,
                             unsigned long nstart, unsigned long ncnt,
                             const HENTRY *ents, unsigned long nents);

static int  compare_lines(const char *a, const char *b);

/* Implementation */
int cfgdiff_compare_paths(const char *old_cfg_path,
                          const char *new_cfg_path,
                          const char *diff_out_path,
                          unsigned int context_lines,
                          CFGDIFF_SUMMARY *out_summary)
{
    char **old_lines = NULL;
    char **new_lines = NULL;
    unsigned long old_n = 0UL, new_n = 0UL;
    int r_old;
    int r_new;
    int old_absent = 0; /* 1 if treated as empty due to NULL/empty/absent */

    unsigned int ctx;

    /* Summary counters */
    unsigned long sum_add = 0UL;
    unsigned long sum_del = 0UL;
    unsigned long sum_chg = 0UL;
    unsigned long grp_add = 0UL;
    unsigned long grp_del = 0UL;

    /* Unified output state */
    FILE *df = NULL;
    HBUF hb;
    PREBUF pb;
    int in_hunk = 0;
    unsigned long h_old_start = 0UL, h_old_cnt = 0UL;
    unsigned long h_new_start = 0UL, h_new_cnt = 0UL;
    unsigned long trail_eq = 0UL;

    /* Line position cursors (1-based, for hunk headers) */
    unsigned long opos = 1UL; /* next old line number to process */
    unsigned long npos = 1UL; /* next new line number to process */

    /* Indices into arrays (0-based) */
    unsigned long i = 0UL, j = 0UL;

    if (out_summary) {
        out_summary->added = 0UL;
        out_summary->removed = 0UL;
        out_summary->changed = 0UL;
    }

    if (new_cfg_path == NULL || new_cfg_path[0] == '\0') {
        return CDIFF_ERR_PARAM;
    }

    ctx = (context_lines == 0U) ? (unsigned int)CDIFF_DEF_CONTEXT : context_lines;
    if (ctx == 0U) {
        ctx = (unsigned int)CDIFF_DEF_CONTEXT; /* safety */
    }

    /* Read new (required) */
    r_new = fu_read_file_lines(new_cfg_path, &new_lines, &new_n);
    if (r_new != FU_OK) {
        return (r_new == FU_ERR_NOMEM) ? CDIFF_ERR_NOMEM :
               (r_new == FU_ERR_PARAM) ? CDIFF_ERR_PARAM :
               (r_new == FU_ERR_STATE) ? CDIFF_ERR_STATE : CDIFF_ERR_IO;
    }

    /* Read old (optional/absent allowed) */
    if (old_cfg_path == NULL || old_cfg_path[0] == '\0') {
        r_old = FU_OK;
        old_lines = NULL;
        old_n = 0UL;
        old_absent = 1;
    } else {
        r_old = fu_read_file_lines(old_cfg_path, &old_lines, &old_n);
        if (r_old != FU_OK) {
            /* Treat any read failure as absent/empty per Phase 2 semantics */
            old_absent = 1;
            r_old = FU_OK;
            old_lines = NULL;
            old_n = 0UL;
        }
    }

    /* Open diff output if requested */
    hb_init(&hb);
    pb_init(&pb, ctx);

    if (diff_out_path != NULL && diff_out_path[0] != '\0') {
        df = fopen(diff_out_path, "w");
        if (df == NULL) {
            fu_free_lines(old_lines, old_n);
            fu_free_lines(new_lines, new_n);
            pb_free(&pb);
            hb_free(&hb);
            return CDIFF_ERR_IO;
        }
        if (!write_udiff_header(df, old_cfg_path, old_absent, new_cfg_path)) {
            fclose(df);
            fu_free_lines(old_lines, old_n);
            fu_free_lines(new_lines, new_n);
            pb_free(&pb);
            hb_free(&hb);
            return CDIFF_ERR_IO;
        }
    }

    /* Main diff loop with modest lookahead alignment */
    while (i < old_n || j < new_n) {
        /* If both have lines and they match, process equality */
        if (i < old_n && j < new_n && compare_lines(old_lines[i], new_lines[j]) == 0) {
            /* Flush any replacement group into summary upon first equality */
            if (grp_add > 0UL || grp_del > 0UL) {
                unsigned long c = (grp_add < grp_del) ? grp_add : grp_del;
                sum_chg += c;
                sum_add += (grp_add - c);
                sum_del += (grp_del - c);
                grp_add = 0UL;
                grp_del = 0UL;
            }

            if (df != NULL) {
                /* Not currently in hunk: record in pre-context ring */
                if (!in_hunk) {
                    pb_push(&pb, new_lines[j]);
                } else {
                    /* In hunk: append context and maybe close after enough */
                    if (!hb_push(&hb, ' ', new_lines[j])) {
                        fclose(df);
                        fu_free_lines(old_lines, old_n);
                        fu_free_lines(new_lines, new_n);
                        pb_free(&pb);
                        hb_free(&hb);
                        return CDIFF_ERR_NOMEM;
                    }
                    h_old_cnt += 1UL;
                    h_new_cnt += 1UL;
                    trail_eq += 1UL;
                    if (trail_eq >= (unsigned long)ctx) {
                        /* Close hunk: write header and body */
                        if (!write_udiff_hunk(df, h_old_start, h_old_cnt, h_new_start, h_new_cnt, hb.v, hb.n)) {
                            fclose(df);
                            fu_free_lines(old_lines, old_n);
                            fu_free_lines(new_lines, new_n);
                            pb_free(&pb);
                            hb_free(&hb);
                            return CDIFF_ERR_IO;
                        }
                        hb_clear(&hb);
                        in_hunk = 0;
                        trail_eq = 0UL;
                        /* Reset pre-context with nothing yet; next '=' will fill */
                        pb.count = 0U;
                        pb.start = 0U;
                    }
                }
            }

            /* Advance on equality */
            i++; j++; opos++; npos++;
            continue;
        }

        /* Handle additions when old is exhausted */
        if (i >= old_n && j < new_n) {
            /* Start a hunk if needed */
            if (df != NULL && !in_hunk) {
                const char **pl;
                unsigned int k, t;
                k = pb_take_last(&pb, ctx, &pl);
                h_old_start = opos - (unsigned long)k;
                h_new_start = npos - (unsigned long)k;
                h_old_cnt = 0UL; h_new_cnt = 0UL; trail_eq = 0UL;
                hb_clear(&hb);
                for (t = 0U; t < k; ++t) {
                    if (!hb_push(&hb, ' ', pl[t])) { free((void*)pl); fclose(df); fu_free_lines(old_lines, old_n); fu_free_lines(new_lines, new_n); pb_free(&pb); hb_free(&hb); return CDIFF_ERR_NOMEM; }
                    h_old_cnt += 1UL; h_new_cnt += 1UL;
                }
                free((void*)pl);
                in_hunk = 1;
            }
            /* Emit additions to hunk or just advance for counts */
            if (df != NULL && in_hunk) {
                if (!hb_push(&hb, '+', new_lines[j])) { if (df) fclose(df); fu_free_lines(old_lines, old_n); fu_free_lines(new_lines, new_n); pb_free(&pb); hb_free(&hb); return CDIFF_ERR_NOMEM; }
                h_new_cnt += 1UL; trail_eq = 0UL;
            }
            grp_add += 1UL;
            j++; npos++;
            continue;
        }

        /* Handle deletions when new is exhausted */
        if (j >= new_n && i < old_n) {
            if (df != NULL && !in_hunk) {
                const char **pl;
                unsigned int k, t;
                k = pb_take_last(&pb, ctx, &pl);
                h_old_start = opos - (unsigned long)k;
                h_new_start = npos - (unsigned long)k;
                h_old_cnt = 0UL; h_new_cnt = 0UL; trail_eq = 0UL;
                hb_clear(&hb);
                for (t = 0U; t < k; ++t) {
                    if (!hb_push(&hb, ' ', pl[t])) { free((void*)pl); if (df) fclose(df); fu_free_lines(old_lines, old_n); fu_free_lines(new_lines, new_n); pb_free(&pb); hb_free(&hb); return CDIFF_ERR_NOMEM; }
                    h_old_cnt += 1UL; h_new_cnt += 1UL;
                }
                free((void*)pl);
                in_hunk = 1;
            }
            if (df != NULL && in_hunk) {
                if (!hb_push(&hb, '-', old_lines[i])) { if (df) fclose(df); fu_free_lines(old_lines, old_n); fu_free_lines(new_lines, new_n); pb_free(&pb); hb_free(&hb); return CDIFF_ERR_NOMEM; }
                h_old_cnt += 1UL; trail_eq = 0UL;
            }
            grp_del += 1UL;
            i++; opos++;
            continue;
        }

        /* Both have lines and differ: apply modest lookahead to resync */
        {
            long ao = -1L; /* index in old where new[j] matches */
            long an = -1L; /* index in new where old[i] matches */
            unsigned int w;
            unsigned long k;

            /* Find in old within window */
            w = 1U;
            while (w <= (unsigned int)CDIFF_LOOKAHEAD && (i + (unsigned long)w) < old_n) {
                if (compare_lines(old_lines[i + (unsigned long)w], new_lines[j]) == 0) { ao = (long)(i + (unsigned long)w); break; }
                w++;
            }
            /* Find in new within window */
            w = 1U;
            while (w <= (unsigned int)CDIFF_LOOKAHEAD && (j + (unsigned long)w) < new_n) {
                if (compare_lines(new_lines[j + (unsigned long)w], old_lines[i]) == 0) { an = (long)(j + (unsigned long)w); break; }
                w++;
            }

            /* Start a hunk if we're going to emit any change */
            if (df != NULL && !in_hunk) {
                const char **pl;
                unsigned int kk, t;
                kk = pb_take_last(&pb, ctx, &pl);
                h_old_start = opos - (unsigned long)kk;
                h_new_start = npos - (unsigned long)kk;
                h_old_cnt = 0UL; h_new_cnt = 0UL; trail_eq = 0UL;
                hb_clear(&hb);
                for (t = 0U; t < kk; ++t) {
                    if (!hb_push(&hb, ' ', pl[t])) { free((void*)pl); if (df) fclose(df); fu_free_lines(old_lines, old_n); fu_free_lines(new_lines, new_n); pb_free(&pb); hb_free(&hb); return CDIFF_ERR_NOMEM; }
                    h_old_cnt += 1UL; h_new_cnt += 1UL;
                }
                free((void*)pl);
                in_hunk = 1;
            }

            if (ao >= 0 && (an < 0 || (unsigned long)ao - i <= (unsigned long)an - j)) {
                /* Prefer deletions to reach match in old */
                k = (unsigned long)ao - i;
                while (k-- > 0UL) {
                    if (df != NULL && in_hunk) {
                        if (!hb_push(&hb, '-', old_lines[i])) { if (df) fclose(df); fu_free_lines(old_lines, old_n); fu_free_lines(new_lines, new_n); pb_free(&pb); hb_free(&hb); return CDIFF_ERR_NOMEM; }
                        h_old_cnt += 1UL; trail_eq = 0UL;
                    }
                    grp_del += 1UL;
                    i++; opos++;
                }
            } else if (an >= 0) {
                /* Prefer additions to reach match in new */
                k = (unsigned long)an - j;
                while (k-- > 0UL) {
                    if (df != NULL && in_hunk) {
                        if (!hb_push(&hb, '+', new_lines[j])) { if (df) fclose(df); fu_free_lines(old_lines, old_n); fu_free_lines(new_lines, new_n); pb_free(&pb); hb_free(&hb); return CDIFF_ERR_NOMEM; }
                        h_new_cnt += 1UL; trail_eq = 0UL;
                    }
                    grp_add += 1UL;
                    j++; npos++;
                }
            } else {
                /* Replace one line: one removal and one addition */
                if (df != NULL && in_hunk) {
                    if (!hb_push(&hb, '-', old_lines[i])) { if (df) fclose(df); fu_free_lines(old_lines, old_n); fu_free_lines(new_lines, new_n); pb_free(&pb); hb_free(&hb); return CDIFF_ERR_NOMEM; }
                    h_old_cnt += 1UL;
                    if (!hb_push(&hb, '+', new_lines[j])) { if (df) fclose(df); fu_free_lines(old_lines, old_n); fu_free_lines(new_lines, new_n); pb_free(&pb); hb_free(&hb); return CDIFF_ERR_NOMEM; }
                    h_new_cnt += 1UL;
                    trail_eq = 0UL;
                }
                grp_del += 1UL;
                grp_add += 1UL;
                i++; j++; opos++; npos++;
            }
            continue;
        }
    }

    /* Flush any pending group into summary at end */
    if (grp_add > 0UL || grp_del > 0UL) {
        unsigned long c = (grp_add < grp_del) ? grp_add : grp_del;
        sum_chg += c;
        sum_add += (grp_add - c);
        sum_del += (grp_del - c);
        grp_add = grp_del = 0UL;
    }

    /* Close any open hunk at end of files (even without full trailing context) */
    if (df != NULL && in_hunk) {
        if (!write_udiff_hunk(df, h_old_start, h_old_cnt, h_new_start, h_new_cnt, hb.v, hb.n)) {
            fclose(df);
            fu_free_lines(old_lines, old_n);
            fu_free_lines(new_lines, new_n);
            pb_free(&pb);
            hb_free(&hb);
            return CDIFF_ERR_IO;
        }
        hb_clear(&hb);
        in_hunk = 0;
    }

    if (df != NULL) {
        if (fclose(df) != 0) {
            fu_free_lines(old_lines, old_n);
            fu_free_lines(new_lines, new_n);
            pb_free(&pb);
            hb_free(&hb);
            return CDIFF_ERR_IO;
        }
    }

    pb_free(&pb);
    hb_free(&hb);

    fu_free_lines(old_lines, old_n);
    fu_free_lines(new_lines, new_n);

    if (out_summary != NULL) {
        out_summary->added = sum_add;
        out_summary->removed = sum_del;
        out_summary->changed = sum_chg;
    }

    return CDIFF_OK;
}

/* === Helper implementations === */

static int compare_lines(const char *a, const char *b)
{
    /* Exact string match; network configs can have significant spacing */
    if (a == b) return 0;
    if (a == NULL || b == NULL) return (a == b) ? 0 : (a ? 1 : -1);
    return strcmp(a, b);
}

static void hb_init(HBUF *hb)
{
    if (hb) { hb->v = NULL; hb->n = 0UL; hb->cap = 0UL; }
}

static int hb_ensure(HBUF *hb, unsigned long need)
{
    HENTRY *nv;
    unsigned long newcap;

    if (hb == NULL) return 0;
    if (hb->cap >= need) return 1;

    newcap = (hb->cap == 0UL) ? 16UL : hb->cap;
    while (newcap < need) {
        if (newcap > 0x7FFFFFFFUL) return 0; /* avoid overflow */
        newcap *= 2UL;
    }

    nv = (HENTRY *)realloc(hb->v, newcap * sizeof(HENTRY));
    if (nv == NULL) return 0;

    hb->v = nv;
    hb->cap = newcap;
    return 1;
}

static void hb_clear(HBUF *hb)
{
    if (hb) hb->n = 0UL;
}

static void hb_free(HBUF *hb)
{
    if (hb && hb->v) { free(hb->v); hb->v = NULL; hb->n = 0UL; hb->cap = 0UL; }
}

static int hb_push(HBUF *hb, char tag, const char *txt)
{
    if (!hb_ensure(hb, hb->n + 1UL)) return 0;
    hb->v[hb->n].tag = tag;
    hb->v[hb->n].text = txt ? txt : "";
    hb->n += 1UL;
    return 1;
}

static void pb_init(PREBUF *pb, unsigned int cap)
{
    if (pb == NULL) return;
    pb->cap = (cap == 0U) ? 1U : cap;
    pb->count = 0U;
    pb->start = 0U;
    pb->v = (const char **)malloc(pb->cap * sizeof(const char *));
    if (pb->v != NULL) {
        unsigned int i;
        for (i = 0U; i < pb->cap; ++i) pb->v[i] = NULL;
    } else {
        pb->cap = 0U;
    }
}

static void pb_free(PREBUF *pb)
{
    if (pb && pb->v) { free((void*)pb->v); pb->v = NULL; pb->cap = 0U; pb->count = 0U; pb->start = 0U; }
}

static void pb_push(PREBUF *pb, const char *txt)
{
    unsigned int pos;
    if (pb == NULL || pb->cap == 0U) return;
    if (pb->count < pb->cap) {
        pos = (pb->start + pb->count) % pb->cap;
        pb->v[pos] = txt;
        pb->count += 1U;
    } else {
        /* overwrite oldest */
        pos = pb->start;
        pb->v[pos] = txt;
        pb->start = (pb->start + 1U) % pb->cap;
    }
}

static unsigned int pb_take_last(PREBUF *pb, unsigned int k, const char ***outp)
{
    unsigned int use, i;
    const char **arr;

    if (outp) *outp = NULL;
    if (pb == NULL || pb->count == 0U) return 0U;

    use = (pb->count < k) ? pb->count : k;

    arr = (const char **)malloc(use * sizeof(const char *));
    if (arr == NULL) {
        return 0U;
    }

    /* Oldest order of the last 'use' entries */
    for (i = 0U; i < use; ++i) {
        unsigned int idx = (pb->start + (pb->count - use) + i) % pb->cap;
        arr[i] = pb->v[idx];
    }

    if (outp) *outp = arr;
    return use;
}

static int write_udiff_header(FILE *f, const char *oldp, int old_absent, const char *newp)
{
    if (f == NULL) return 0;
    if (fprintf(f, "--- %s\n", (oldp && !old_absent) ? oldp : "(absent)") < 0) return 0;
    if (fprintf(f, "+++ %s\n", (newp && newp[0] != '\0') ? newp : "(new)") < 0) return 0;
    return 1;
}

static int write_udiff_hunk(FILE *f,
                            unsigned long ostart, unsigned long ocnt,
                            unsigned long nstart, unsigned long ncnt,
                            const HENTRY *ents, unsigned long nents)
{
    unsigned long i;
    if (f == NULL) return 0;

    if (fprintf(f, "@@ -%lu,%lu +%lu,%lu @@\n",
                (unsigned long)ostart, (unsigned long)ocnt,
                (unsigned long)nstart, (unsigned long)ncnt) < 0) {
        return 0;
    }

    for (i = 0UL; i < nents; ++i) {
        int rc;
        char t = ents[i].tag;
        const char *s = ents[i].text ? ents[i].text : "";
        rc = fprintf(f, "%c%s\n", t, s);
        if (rc < 0) return 0;
    }
    return 1;
}
