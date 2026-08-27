/*
 * STORAGE.C - Phase 1 inventory storage implementation
 * DEC C / OpenVMS C89 compatible
 * Simple local text-file config ingest plus persistent
 * inventory storage and basic list/show retrieval.
 * No networking, diffing, rules, or scoring.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#include "storage.h"

/* Internal limits */
#define ST_PATH_MAX     512
#define ST_LINE_MAX     2048
#define ST_GROW_STEP    8

/* Internal store representation */
struct inv_store {
    char store_path[ST_PATH_MAX];
    INV_ITEM *items;
    unsigned long count;
    unsigned long cap;
    int dirty;
};

/* Prototypes for internal helpers */
static int st_file_exists(const char *p);
static int st_copy_file(const char *src, const char *dst, unsigned long *out_sz);
static int st_write_index(INV_STORE *st);
static int st_read_index(INV_STORE *st);
static void st_free_all(INV_STORE *st);
static int st_ensure_cap(INV_STORE *st, unsigned long need);
static void st_sanitize_copy(char *dst, size_t dlen, const char *src);
static void st_make_cfg_path(char *out, unsigned int outlen, const char *base,
                             const char *device_id);
static int st_find_index(INV_STORE *st, const char *device_id, unsigned long *idx_out);

/* Implementation */
INV_STORE *inv_open(const char *path, int create_new)
{
    INV_STORE *st;
    FILE *f;
    size_t plen;

    if (path == NULL) {
        return NULL;
    }

    plen = strlen(path);
    if (plen == 0 || plen >= ST_PATH_MAX) {
        return NULL;
    }

    if (create_new) {
        /* Truncate or create a new empty index file */
        f = fopen(path, "wb");
        if (f == NULL) {
            return NULL;
        }
        fclose(f);
    } else {
        /* Ensure the file exists; if not, create empty */
        f = fopen(path, "rb");
        if (f == NULL) {
            f = fopen(path, "wb");
            if (f == NULL) {
                return NULL;
            }
        }
        fclose(f);
    }

    st = (INV_STORE *)malloc(sizeof(*st));
    if (st == NULL) {
        return NULL;
    }

    memset(st, 0, sizeof(*st));
    strncpy(st->store_path, path, sizeof(st->store_path) - 1);
    st->store_path[sizeof(st->store_path) - 1] = '\0';
    st->items = NULL;
    st->count = 0;
    st->cap = 0;
    st->dirty = 0;

    /* Read existing index, if any */
    if (st_read_index(st) != INV_OK) {
        /* Corrupt or unreadable index; start empty but keep file as is */
        /* Caller can decide how to proceed */
        /* We do not fail open to keep things simple */
        st->count = 0;
        st->cap = 0;
        if (st->items) {
            free(st->items);
            st->items = NULL;
        }
        st->dirty = 0;
    }

    return st;
}

void inv_close(INV_STORE *st)
{
    if (st == NULL) {
        return;
    }

    if (st->dirty) {
        (void)st_write_index(st);
    }

    st_free_all(st);
    free(st);
}

int inv_sync(INV_STORE *st)
{
    if (st == NULL) {
        return INV_ERR_PARAM;
    }
    if (!st->dirty) {
        return INV_OK;
    }
    return st_write_index(st);
}

int inv_add_file(INV_STORE *st,
                 const char *device_id,
                 const char *mgmt_addr,
                 const char *cfg_file_path)
{
    unsigned long idx;
    int have_idx;
    INV_ITEM it;
    char dest_path[INV_MAX_PATH_LEN];
    unsigned long sz;
    time_t tt;

    if (st == NULL || device_id == NULL || cfg_file_path == NULL) {
        return INV_ERR_PARAM;
    }
    if (device_id[0] == '\0') {
        return INV_ERR_PARAM;
    }

    /* Prepare item */
    memset(&it, 0, sizeof(it));
    st_sanitize_copy(it.device_id, sizeof(it.device_id), device_id);
    if (mgmt_addr != NULL) {
        st_sanitize_copy(it.mgmt_addr, sizeof(it.mgmt_addr), mgmt_addr);
    } else {
        it.mgmt_addr[0] = '\0';
    }

    st_make_cfg_path(dest_path, sizeof(dest_path), st->store_path, it.device_id);

    if (dest_path[0] == '\0') {
        return INV_ERR_IO;
    }

    if (st_copy_file(cfg_file_path, dest_path, &sz) != INV_OK) {
        return INV_ERR_IO;
    }

    strncpy(it.cfg_path, dest_path, sizeof(it.cfg_path) - 1);
    it.cfg_path[sizeof(it.cfg_path) - 1] = '\0';
    it.cfg_size = sz;
    tt = time((time_t *)0);
    if (tt == (time_t)-1) {
        it.updated_time = 0;
    } else {
        it.updated_time = (unsigned long)tt;
    }

    /* Find existing index */
    have_idx = st_find_index(st, it.device_id, &idx);

    if (have_idx) {
        st->items[idx] = it;
    } else {
        if (st_ensure_cap(st, st->count + 1) != INV_OK) {
            return INV_ERR_NOMEM;
        }
        st->items[st->count] = it;
        st->count++;
    }

    st->dirty = 1;
    return INV_OK;
}

unsigned long inv_count(INV_STORE *st)
{
    if (st == NULL) {
        return 0UL;
    }
    return st->count;
}

int inv_get_by_index(INV_STORE *st, unsigned long index, INV_ITEM *out)
{
    if (st == NULL || out == NULL) {
        return INV_ERR_PARAM;
    }
    if (index >= st->count) {
        return INV_ERR_NOTFOUND;
    }
    *out = st->items[index];
    return INV_OK;
}

int inv_get_by_id(INV_STORE *st, const char *device_id, INV_ITEM *out)
{
    unsigned long i;

    if (st == NULL || device_id == NULL || out == NULL) {
        return INV_ERR_PARAM;
    }
    for (i = 0; i < st->count; ++i) {
        if (strcmp(st->items[i].device_id, device_id) == 0) {
            *out = st->items[i];
            return INV_OK;
        }
    }
    return INV_ERR_NOTFOUND;
}

int inv_get_cfg_path(INV_STORE *st,
                     const char *device_id,
                     char *out_path,
                     unsigned int out_len)
{
    unsigned long i;
    size_t need;

    if (st == NULL || device_id == NULL || out_path == NULL || out_len == 0U) {
        return INV_ERR_PARAM;
    }

    for (i = 0; i < st->count; ++i) {
        if (strcmp(st->items[i].device_id, device_id) == 0) {
            need = strlen(st->items[i].cfg_path);
            if (need + 1 > out_len) {
                return INV_ERR_PARAM;
            }
            strncpy(out_path, st->items[i].cfg_path, out_len - 1);
            out_path[out_len - 1] = '\0';
            return INV_OK;
        }
    }

    return INV_ERR_NOTFOUND;
}

/* Internal helpers */
static int st_file_exists(const char *p)
{
    FILE *f;
    if (p == NULL) {
        return 0;
    }
    f = fopen(p, "rb");
    if (f != NULL) {
        fclose(f);
        return 1;
    }
    return 0;
}

static int st_copy_file(const char *src, const char *dst, unsigned long *out_sz)
{
    FILE *fi = NULL;
    FILE *fo = NULL;
    unsigned char buf[4096];
    size_t n;
    unsigned long total = 0UL;

    if (out_sz) {
        *out_sz = 0UL;
    }

    fi = fopen(src, "r");
    if (fi == NULL) {
        return INV_ERR_IO;
    }

    fo = fopen(dst, "w");
    if (fo == NULL) {
        fclose(fi);
        return INV_ERR_IO;
    }

    while ((n = fread(buf, 1, sizeof(buf), fi)) > 0) {
        size_t w = fwrite(buf, 1, n, fo);
        if (w != n) {
            fclose(fi);
            fclose(fo);
            (void)remove(dst);
            return INV_ERR_IO;
        }
        total += (unsigned long)w;
    }

    if (ferror(fi)) {
        fclose(fi);
        fclose(fo);
        (void)remove(dst);
        return INV_ERR_IO;
    }

    fclose(fi);
    if (fclose(fo) != 0) {
        (void)remove(dst);
        return INV_ERR_IO;
    }

    if (out_sz) {
        *out_sz = total;
    }

    return INV_OK;
}

static void st_make_cfg_path(char *out, unsigned int outlen, const char *base,
                             const char *device_id)
{
    /*
     * Build a config file path derived from the base store path and device id.
     * We avoid introducing extra '.' to remain friendly to ODS-2 file systems.
     * Format: base + "__" + escaped(device_id) + "_cfg"
     */
    char esc[INV_MAX_ID_LEN];
    size_t blen, need;
    unsigned int i, j;

    if (out == NULL || outlen == 0U) {
        return;
    }
    out[0] = '\0';

    if (base == NULL || device_id == NULL) {
        return;
    }

    /* Escape device_id: keep A-Z a-z 0-9 and '-' '_' only; others -> '_' */
    j = 0U;
    for (i = 0U; device_id[i] != '\0' && j + 1U < sizeof(esc); ++i) {
        int c = (unsigned char)device_id[i];
        if ((c >= 'A' && c <= 'Z') ||
            (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') ||
            c == '-' || c == '_') {
            esc[j++] = (char)c;
        } else {
            esc[j++] = '_';
        }
    }
    esc[j] = '\0';

    blen = strlen(base);
    need = blen + 2 + strlen(esc) + 4 + 1; /* base + "__" + esc + "_cfg" + NUL */

    if (need > outlen) {
        /* Truncate escaped id if needed */
        size_t room = (outlen > (unsigned int)(blen + 2 + 4 + 1)) ? (outlen - (blen + 2 + 4 + 1)) : 0;
        if (room == 0) {
            out[0] = '\0';
            return;
        }
        esc[room - 1] = '\0';
    }

    /* Build the path */
    strncpy(out, base, outlen - 1);
    out[outlen - 1] = '\0';

    if (strlen(out) + 2 < outlen) {
        strcat(out, "__");
    } else {
        out[0] = '\0';
        return;
    }

    if (strlen(out) + strlen(esc) < outlen) {
        strcat(out, esc);
    } else {
        out[0] = '\0';
        return;
    }

    if (strlen(out) + 4 < outlen) {
        strcat(out, "_cfg");
    } else {
        out[0] = '\0';
        return;
    }
}

static int st_find_index(INV_STORE *st, const char *device_id, unsigned long *idx_out)
{
    unsigned long i;
    if (idx_out) {
        *idx_out = 0UL;
    }
    if (st == NULL || device_id == NULL) {
        return 0;
    }
    for (i = 0; i < st->count; ++i) {
        if (strcmp(st->items[i].device_id, device_id) == 0) {
            if (idx_out) {
                *idx_out = i;
            }
            return 1;
        }
    }
    return 0;
}

static int st_ensure_cap(INV_STORE *st, unsigned long need)
{
    unsigned long newcap;
    INV_ITEM *ni;

    if (st->cap >= need) {
        return INV_OK;
    }

    newcap = st->cap ? st->cap : ST_GROW_STEP;
    while (newcap < need) {
        if (newcap > 0xFFFFFFFFUL / 2UL) {
            newcap = need; /* last resort */
            break;
        }
        newcap *= 2UL;
    }

    ni = (INV_ITEM *)realloc(st->items, newcap * sizeof(INV_ITEM));
    if (ni == NULL) {
        return INV_ERR_NOMEM;
    }

    st->items = ni;
    st->cap = newcap;
    return INV_OK;
}

static void st_sanitize_copy(char *dst, size_t dlen, const char *src)
{
    size_t i, j;
    if (dst == NULL || dlen == 0) {
        return;
    }
    if (src == NULL) {
        dst[0] = '\0';
        return;
    }
    j = 0;
    for (i = 0; src[i] != '\0' && j + 1 < dlen; ++i) {
        unsigned char c = (unsigned char)src[i];
        if (c == '\r' || c == '\n' || c == '\t') {
            dst[j++] = ' ';
        } else {
            dst[j++] = (char)c;
        }
    }
    dst[j] = '\0';
}

static int st_write_index(INV_STORE *st)
{
    char tmp_path[ST_PATH_MAX];
    FILE *f;
    unsigned long i;

    if (st == NULL) {
        return INV_ERR_PARAM;
    }

    /* Build temporary path: base + "_TMP" */
    if (strlen(st->store_path) + 4 >= sizeof(tmp_path)) {
        return INV_ERR_IO;
    }
    strcpy(tmp_path, st->store_path);
    strcat(tmp_path, "_TMP");

    f = fopen(tmp_path, "wb");
    if (f == NULL) {
        return INV_ERR_IO;
    }

    /* Optional header */
    /* fprintf(f, "#INV1\n"); */

    for (i = 0; i < st->count; ++i) {
        INV_ITEM *it = &st->items[i];
        if (fprintf(f, "%s\t%s\t%s\t%lu\t%lu\n",
                    it->device_id,
                    it->mgmt_addr,
                    it->cfg_path,
                    (unsigned long)it->cfg_size,
                    (unsigned long)it->updated_time) < 0) {
            fclose(f);
            (void)remove(tmp_path);
            return INV_ERR_IO;
        }
    }

    if (fclose(f) != 0) {
        (void)remove(tmp_path);
        return INV_ERR_IO;
    }

    /* Replace original */
    (void)remove(st->store_path);
    if (rename(tmp_path, st->store_path) != 0) {
        (void)remove(tmp_path);
        return INV_ERR_IO;
    }

    st->dirty = 0;
    return INV_OK;
}

static int st_read_index(INV_STORE *st)
{
    FILE *f;
    char line[ST_LINE_MAX];

    if (st == NULL) {
        return INV_ERR_PARAM;
    }

    f = fopen(st->store_path, "rb");
    if (f == NULL) {
        return INV_ERR_IO;
    }

    while (fgets(line, sizeof(line), f) != NULL) {
        char *p = line;
        char *t1, *t2, *t3, *t4, *t5;
        char *nl;
        INV_ITEM it;
        unsigned long idx;

        /* Trim trailing newline */
        nl = strchr(p, '\n');
        if (nl) *nl = '\0';

        if (p[0] == '\0') continue;
        if (p[0] == '#') continue;

        t1 = p;
        t2 = strchr(t1, '\t');
        if (t2 == NULL) continue; *t2++ = '\0';
        t3 = strchr(t2, '\t');
        if (t3 == NULL) continue; *t3++ = '\0';
        t4 = strchr(t3, '\t');
        if (t4 == NULL) continue; *t4++ = '\0';
        t5 = strchr(t4, '\t');
        if (t5 == NULL) continue; *t5++ = '\0';

        memset(&it, 0, sizeof(it));
        st_sanitize_copy(it.device_id, sizeof(it.device_id), t1);
        st_sanitize_copy(it.mgmt_addr, sizeof(it.mgmt_addr), t2);
        st_sanitize_copy(it.cfg_path, sizeof(it.cfg_path), t3);
        it.cfg_size = (unsigned long)strtoul(t4, (char **)0, 10);
        it.updated_time = (unsigned long)strtoul(t5, (char **)0, 10);

        if (st_find_index(st, it.device_id, &idx)) {
            st->items[idx] = it;
        } else {
            if (st_ensure_cap(st, st->count + 1) != INV_OK) {
                fclose(f);
                return INV_ERR_NOMEM;
            }
            st->items[st->count] = it;
            st->count++;
        }
    }

    fclose(f);
    st->dirty = 0;
    return INV_OK;
}

static void st_free_all(INV_STORE *st)
{
    if (st == NULL) return;
    if (st->items) {
        free(st->items);
        st->items = NULL;
    }
    st->count = 0;
    st->cap = 0;
    st->dirty = 0;
}
