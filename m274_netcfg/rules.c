/*
 * RULES.C - M274 Phase 2 first-pass rule engine
 * DEC C / OpenVMS C89 compatible
 *
 * Implements RULES.H using FILEUTIL for text I/O. Provides:
 *  - bounded rule context and results (no dynamic sizing of public arrays)
 *  - built-in defaults when no rules file is supplied
 *  - optional rules-file parsing: require/forbid x warn/error
 *  - simple substring scanning (case-sensitive, C89 strstr semantics)
 *  - warning/error totals and a first-issue message via last_error
 *
 * Omitted by design: regex, scoring, reporting, Phase 3 behavior.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "rules.h"
#include "fileutil.h"
#include <time.h>

/* ---------- Internal helpers (file-local) ---------- */

static void r_set_err(struct rule_ctx *ctx, const char *msg)
{
    size_t n;
    if (ctx == NULL) return;
    if (msg == NULL) {
        ctx->last_error[0] = '\0';
        return;
    }
    n = strlen(msg);
    if (n >= sizeof(ctx->last_error)) n = sizeof(ctx->last_error) - 1u;
    memcpy(ctx->last_error, msg, n);
    ctx->last_error[n] = '\0';
}

static void r_set_err2(struct rule_ctx *ctx, const char *pfx, const char *msg)
{
    char buf[sizeof(((struct rule_ctx *)0)->last_error)];
    size_t lp, lm, m;
    if (ctx == NULL) return;
    if (pfx == NULL) pfx = "";
    if (msg == NULL) msg = "";
    lp = strlen(pfx);
    lm = strlen(msg);
    m = 0u;
    if (lp + lm >= sizeof(buf)) {
        if (lp >= sizeof(buf)) lp = sizeof(buf) - 1u;
        lm = sizeof(buf) - 1u - lp;
    }
    if (lp > 0u) {
        memcpy(buf + m, pfx, lp);
        m += lp;
    }
    if (lm > 0u) {
        memcpy(buf + m, msg, lm);
        m += lm;
    }
    buf[m] = '\0';
    r_set_err(ctx, buf);
}

static int ci_eq(const char *a, const char *b)
{
    unsigned char ca, cb;
    if (a == NULL || b == NULL) return 0;
    while (*a && *b) {
        ca = (unsigned char)*a;
        cb = (unsigned char)*b;
        if (tolower(ca) != tolower(cb)) return 0;
        ++a; ++b;
    }
    return (*a == '\0' && *b == '\0');
}

static void trim_ws(char *s)
{
    char *e;
    size_t n;
    if (s == NULL) return;
    /* left trim */
    while (*s && (unsigned char)*s <= ' ') {
        memmove(s, s + 1, strlen(s));
    }
    /* right trim */
    n = strlen(s);
    while (n > 0u) {
        e = s + n - 1u;
        if ((unsigned char)*e <= ' ') {
            *e = '\0';
            --n;
        } else {
            break;
        }
    }
}

static unsigned int uminu(unsigned int a, unsigned int b)
{
    return a < b ? a : b;
}

static void rr_fill(struct rule_result *rr,
                    unsigned short rindex,
                    unsigned char rtype,
                    unsigned char rsev,
                    unsigned long line_no,
                    unsigned long col_no,
                    const char *substr,
                    int *ptrunc)
{
    size_t sl;
    rr->rule_index = rindex;
    rr->type = rtype;
    rr->sev = rsev;
    rr->line_no = line_no;
    rr->col_no = col_no;
    rr->match[0] = '\0';
    if (substr != NULL) {
        sl = strlen(substr);
        if (sl >= (size_t)RULES_MAX_SUBSTR) {
            if (ptrunc) *ptrunc = 1;
            sl = (size_t)RULES_MAX_SUBSTR - 1u;
        }
        memcpy(rr->match, substr, sl);
        rr->match[sl] = '\0';
    }
}

static void compose_first_issue(char *out, size_t outsz,
                                const struct rule_result *first,
                                unsigned long ecnt,
                                unsigned long wcnt)
{
    const char *sev = (first && first->sev == RULE_SEV_ERROR) ? "ERROR" : "WARN";
    const char *typ = (first && first->type == RULE_TYPE_FORBID) ? "forbid" : "require";
    char loc[48];
    size_t m = 0u, n;
    if (out == NULL || outsz == 0u) return;
    out[0] = '\0';

    if (first != NULL) {
        if (first->line_no > 0UL) {
            sprintf(loc, "line %lu col %lu", first->line_no, first->col_no);
        } else {
            strcpy(loc, "missing");
        }
        /* Format: "ERROR forbid '<match>' at line X col Y (errors=E warnings=W)" */
        /* Start with sev */
        n = strlen(sev);
        if (m + n + 1u < outsz) { memcpy(out + m, sev, n); m += n; out[m++] = ' '; }
        /* type */
        n = strlen(typ);
        if (m + n + 1u < outsz) { memcpy(out + m, typ, n); m += n; out[m++] = ' '; }
        /* 'match' */
        if (m + 1u < outsz) out[m++] = '\'';
        if (first->match[0]) {
            n = strlen(first->match);
            if (m + n < outsz) { memcpy(out + m, first->match, n); m += n; }
            else { /* truncate */ n = outsz - 1u - m; memcpy(out + m, first->match, n); m += n; }
        }
        if (m + 2u < outsz) { out[m++] = '\''; out[m++] = ' '; }
        /* at */
        n = strlen("at ");
        if (m + n < outsz) { memcpy(out + m, "at ", n); m += n; }
        /* loc */
        n = strlen(loc);
        if (m + n < outsz) { memcpy(out + m, loc, n); m += n; }
        /* space */
        if (m + 1u < outsz) out[m++] = ' ';
    }
    {
        char tail[64];
        sprintf(tail, "(errors=%lu warnings=%lu)", ecnt, wcnt);
        n = strlen(tail);
        if (m + n >= outsz) n = outsz - 1u - m;
        if (n > 0u) { memcpy(out + m, tail, n); m += n; }
        out[m] = '\0';
    }
}

static int map_fu_err(int fr)
{
    switch (fr) {
    case FU_OK: return RULES_OK;
    case FU_ERR_PARAM: return RULES_ERR_PARAM;
    case FU_ERR_IO: return RULES_ERR_IO;
    case FU_ERR_NOMEM: return RULES_ERR_STATE;
    case FU_ERR_STATE: return RULES_ERR_STATE;
    default: return RULES_ERR_STATE;
    }
}

/* ---------- Public API implementation ---------- */

int rules_ctx_init_def(struct rule_ctx *ctx)
{
    int rc, agg_trunc;
    if (ctx == NULL) return RULES_ERR_PARAM;
    rules_ctx_reset(ctx);
    agg_trunc = 0;

    /* Conservative, generic defaults for first pass */
    rc = rules_add_rule(ctx, RULE_TYPE_FORBID, RULE_SEV_ERROR, "PRIVATE KEY");
    if (rc == RULES_TRUNC) agg_trunc = 1; else if (rc != RULES_OK) return rc;

    rc = rules_add_rule(ctx, RULE_TYPE_FORBID, RULE_SEV_WARN, "TODO");
    if (rc == RULES_TRUNC) agg_trunc = 1; else if (rc != RULES_OK) return rc;

    rc = rules_add_rule(ctx, RULE_TYPE_REQUIRE, RULE_SEV_WARN, "hostname");
    if (rc == RULES_TRUNC) agg_trunc = 1; else if (rc != RULES_OK) return rc;

    r_set_err(ctx, "");
    return agg_trunc ? RULES_TRUNC : RULES_OK;
}

void rules_ctx_reset(struct rule_ctx *ctx)
{
    unsigned int i;
    if (ctx == NULL) return;
    ctx->rule_count = 0u;
    ctx->flags = 0ul;
    for (i = 0u; i < (unsigned int)RULES_MAX_RULES; ++i) {
        ctx->rules[i].in_use = 0u;
        ctx->rules[i].type = 0u;
        ctx->rules[i].sev = 0u;
        ctx->rules[i].substr[0] = '\0';
    }
    r_set_err(ctx, "");
}

void rules_ctx_free(struct rule_ctx *ctx)
{
    /* No dynamic allocations owned by ctx in this bounded design */
    rules_ctx_reset(ctx);
}

int rules_add_rule(struct rule_ctx *ctx,
                   unsigned char type,
                   unsigned char sev,
                   const char *substr)
{
    struct rule_def *rd;
    size_t sl;
    int trunc = 0;

    if (ctx == NULL || substr == NULL) return RULES_ERR_PARAM;
    if (!(type == RULE_TYPE_REQUIRE || type == RULE_TYPE_FORBID)) return RULES_ERR_PARAM;
    if (!(sev == RULE_SEV_WARN || sev == RULE_SEV_ERROR)) return RULES_ERR_PARAM;

    if (ctx->rule_count >= (unsigned int)RULES_MAX_RULES) {
        r_set_err2(ctx, "rules full: ", "cannot add more");
        return RULES_ERR_FULL;
    }

    rd = &ctx->rules[ctx->rule_count];
    rd->in_use = 1u;
    rd->type = type;
    rd->sev = sev;

    sl = strlen(substr);
    if (sl >= (size_t)RULES_MAX_SUBSTR) {
        sl = (size_t)RULES_MAX_SUBSTR - 1u;
        trunc = 1;
    }
    memcpy(rd->substr, substr, sl);
    rd->substr[sl] = '\0';

    ctx->rule_count += 1u;
    if (trunc) {
        r_set_err2(ctx, "substring truncated: ", substr);
        return RULES_TRUNC;
    }
    return RULES_OK;
}

int rules_load_text(struct rule_ctx *ctx,
                    const char *path,
                    int replace_existing)
{
    char **lines = NULL;
    unsigned long lcount = 0UL;
    unsigned long i;
    int fr, rc, agg_trunc, any_added;

    if (ctx == NULL || path == NULL) return RULES_ERR_PARAM;

    fr = fu_read_file_lines(path, &lines, &lcount);
    if (fr != FU_OK) {
        r_set_err2(ctx, "rules file read error: ", path);
        return map_fu_err(fr);
    }

    if (replace_existing) {
        rules_ctx_reset(ctx);
    }

    agg_trunc = 0;
    any_added = 0;

    for (i = 0UL; i < lcount; ++i) {
        char *ln = lines[i];
        char *c1, *c2;
        char *tok_type, *tok_sev, *tok_sub;
        unsigned char rtype, rsev;

        if (ln == NULL) continue;
        trim_ws(ln);
        if (ln[0] == '\0') continue; /* empty */
        if (ln[0] == '#') continue;   /* comment */

        c1 = strchr(ln, ',');
        if (c1 == NULL) {
            fu_free_lines(lines, lcount);
            r_set_err2(ctx, "parse error (missing comma) at line ", "");
            return RULES_ERR_PARSE;
        }
        *c1 = '\0';
        c2 = strchr(c1 + 1, ',');
        if (c2 == NULL) {
            fu_free_lines(lines, lcount);
            r_set_err2(ctx, "parse error (missing second comma) at line ", "");
            return RULES_ERR_PARSE;
        }
        *c2 = '\0';

        tok_type = ln;
        tok_sev = c1 + 1;
        tok_sub = c2 + 1;
        trim_ws(tok_type);
        trim_ws(tok_sev);
        /* For substring, trim leading/trailing ASCII spaces only, keep inner */
        while (*tok_sub && (unsigned char)*tok_sub <= ' ') tok_sub++;
        {
            size_t tlen = strlen(tok_sub);
            while (tlen > 0u && (unsigned char)tok_sub[tlen - 1u] <= ' ') {
                tok_sub[tlen - 1u] = '\0';
                --tlen;
            }
        }

        if (ci_eq(tok_type, "require")) rtype = RULE_TYPE_REQUIRE;
        else if (ci_eq(tok_type, "forbid")) rtype = RULE_TYPE_FORBID;
        else {
            fu_free_lines(lines, lcount);
            r_set_err2(ctx, "parse error (type) at line ", "");
            return RULES_ERR_PARSE;
        }

        if (ci_eq(tok_sev, "warn") || ci_eq(tok_sev, "warning")) rsev = RULE_SEV_WARN;
        else if (ci_eq(tok_sev, "error")) rsev = RULE_SEV_ERROR;
        else {
            fu_free_lines(lines, lcount);
            r_set_err2(ctx, "parse error (severity) at line ", "");
            return RULES_ERR_PARSE;
        }

        rc = rules_add_rule(ctx, rtype, rsev, tok_sub);
        if (rc == RULES_OK) {
            any_added = 1;
        } else if (rc == RULES_TRUNC) {
            any_added = 1;
            agg_trunc = 1;
        } else {
            /* Full or param error: stop */
            fu_free_lines(lines, lcount);
            return rc;
        }
    }

    fu_free_lines(lines, lcount);

    if (!any_added && replace_existing) {
        /* Nothing added; keep context but note */
        r_set_err(ctx, "no rules loaded");
    } else {
        r_set_err(ctx, "");
    }

    return agg_trunc ? RULES_TRUNC : RULES_OK;
}

int rules_check_config(struct rule_ctx *ctx,
                       const char *cfg_path,
                       struct rule_result *resv,
                       unsigned int *resn)
{
    char **lines = NULL;
    unsigned long lcount = 0UL;
    unsigned long li;
    unsigned int cap, outc;
    unsigned int i;
    int fr;
    int trunc = 0;
    unsigned long warn_total = 0UL, err_total = 0UL;
    int have_first = 0;
    struct rule_result first_rr; /* local copy for first issue */

if (ctx == NULL || cfg_path == NULL || resv == NULL || resn == NULL) {
        return RULES_ERR_PARAM;
    }

    cap = *resn;
    if (cap > (unsigned int)RULES_MAX_RESULTS) cap = (unsigned int)RULES_MAX_RESULTS;
    *resn = 0u;

    fr = fu_read_file_lines(cfg_path, &lines, &lcount);
    if (fr != FU_OK) {
        r_set_err2(ctx, "config read error: ", cfg_path);
        return map_fu_err(fr);
    }

    outc = 0u;
    for (i = 0u; i < ctx->rule_count; ++i) {
        struct rule_def *rd = &ctx->rules[i];
        if (!rd->in_use) continue;

        if (rd->type == RULE_TYPE_FORBID) {
            for (li = 0UL; li < lcount; ++li) {
                char *line = lines[li];
                char *q, *fnd;
                size_t ofs;
                if (line == NULL || rd->substr[0] == '\0') continue;
                q = line;
                for (;;) {
                    fnd = strstr(q, rd->substr);
                    if (fnd == NULL) break;
                    ofs = (size_t)(fnd - line);
                    if (rd->sev == RULE_SEV_ERROR) ++err_total; else ++warn_total;
                    if (outc < cap) {
                        rr_fill(&resv[outc], (unsigned short)i, rd->type, rd->sev,
                                (unsigned long)(li + 1UL), (unsigned long)(ofs + 1UL), rd->substr, &trunc);
                        if (!have_first) { first_rr = resv[outc]; have_first = 1; }
                        outc++;
                    } else if (!have_first) {
                        /* Capture first even if not stored */
                        rr_fill(&first_rr, (unsigned short)i, rd->type, rd->sev,
                                (unsigned long)(li + 1UL), (unsigned long)(ofs + 1UL), rd->substr, &trunc);
                        have_first = 1;
                    }
                    if (*(fnd) != '\0') q = fnd + 1; else break; /* progress */
                }
            }
        } else if (rd->type == RULE_TYPE_REQUIRE) {
            int found = 0;
            unsigned long found_line = 0UL, found_col = 0UL;
            for (li = 0UL; li < lcount && !found; ++li) {
                char *line = lines[li];
                char *fnd;
                size_t ofs;
                if (line == NULL || rd->substr[0] == '\0') continue;
                fnd = strstr(line, rd->substr);
                if (fnd != NULL) {
                    ofs = (size_t)(fnd - line);
                    found = 1;
                    found_line = (unsigned long)(li + 1UL);
                    found_col = (unsigned long)(ofs + 1UL);
                }
            }
            if (!found) {
                if (rd->sev == RULE_SEV_ERROR) ++err_total; else ++warn_total;
                if (outc < cap) {
                    rr_fill(&resv[outc], (unsigned short)i, rd->type, rd->sev,
                            0UL, 0UL, rd->substr, &trunc);
                    if (!have_first) { first_rr = resv[outc]; have_first = 1; }
                    outc++;
                } else if (!have_first) {
                    rr_fill(&first_rr, (unsigned short)i, rd->type, rd->sev,
                            0UL, 0UL, rd->substr, &trunc);
                    have_first = 1;
                }
            }
        }
    }

    fu_free_lines(lines, lcount);

    *resn = outc;

    /* Compose last_error message summarizing first issue and totals */
    if (have_first) {
        char msg[sizeof(((struct rule_ctx *)0)->last_error)];
        compose_first_issue(msg, sizeof(msg), &first_rr, err_total, warn_total);
        r_set_err(ctx, msg);
    } else {
        r_set_err(ctx, "");
    }

    return trunc ? RULES_TRUNC : RULES_OK;
}

const char *rules_last_error(const struct rule_ctx *ctx)
{
    static const char empty[] = "";
    if (ctx == NULL) return empty;
    return ctx->last_error;
}

 void report_risk_score(
    unsigned int error_count,
    unsigned int warn_count)
{
    {
        unsigned int risk_score;
        unsigned int rs;
        const char *risk_level;
    
        rs = (error_count * 40U) + (warn_count * 10U);
        risk_score = (rs > 100U) ? 100U : rs;
    
        if (risk_score <= 19U) {
            risk_level = "LOW";
        } else if (risk_score <= 49U) {
            risk_level = "MEDIUM";
        } else if (risk_score <= 79U) {
            risk_level = "HIGH";
        } else {
            risk_level = "CRITICAL";
        }
    
        printf("Risk score: %u/100 (%s)\n", risk_score, risk_level);
    }
}

