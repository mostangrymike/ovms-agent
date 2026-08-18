#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include "cobol_guard.h"

#define COB_NAME_MAX 32U
#define COB_NAME_COUNT 64U

typedef struct cob_scope_count {
    long if_open;
    long if_close;
    long eval_open;
    long eval_close;
    long search_open;
    long search_close;
    char para[COB_NAME_COUNT][COB_NAME_MAX];
    unsigned int para_count[COB_NAME_COUNT];
    unsigned int para_used;
    char section[COB_NAME_COUNT][COB_NAME_MAX];
    unsigned int section_count[COB_NAME_COUNT];
    unsigned int section_used;
} cob_scope_count;

static int cob_eq_ci(const char *a, const char *b)
{
    unsigned char ca;
    unsigned char cb;

    if (a == NULL || b == NULL) return 0;
    while (*a != '\0' && *b != '\0') {
        ca = (unsigned char)toupper((unsigned char)*a++);
        cb = (unsigned char)toupper((unsigned char)*b++);
        if (ca != cb) return 0;
    }
    return *a == '\0' && *b == '\0';
}

static int cob_ext(const char *path)
{
    const char *dot;
    const char *semi;
    char ext[8];
    size_t used;

    if (path == NULL) return 0;
    dot = strrchr(path, '.');
    if (dot == NULL) return 0;
    ++dot;
    semi = strchr(dot, ';');
    used = semi == NULL ? strlen(dot) : (size_t)(semi - dot);
    if (used == 0U || used >= sizeof(ext)) return 0;
    (void)memcpy(ext, dot, used);
    ext[used] = '\0';
    return cob_eq_ci(ext, "COB") ||
           cob_eq_ci(ext, "COBOL") ||
           cob_eq_ci(ext, "CBL");
}

static void cob_reason(char *reason, size_t size, const char *text)
{
    if (reason == NULL || size == 0U) return;
    (void)strncpy(reason, text != NULL ? text : "", size - 1U);
    reason[size - 1U] = '\0';
}

static int cob_word_char(unsigned char ch)
{
    return isalnum((int)ch) || ch == (unsigned char)'-' ||
           ch == (unsigned char)'_';
}

static int cob_token_at(const char *line, const char *word)
{
    size_t n;
    size_t i;

    if (line == NULL || word == NULL) return 0;
    while (*line == ' ' || *line == '\t') ++line;
    n = strlen(word);
    for (i = 0U; i < n; ++i) {
        if (line[i] == '\0' ||
            toupper((unsigned char)line[i]) !=
            toupper((unsigned char)word[i])) return 0;
    }
    return !cob_word_char((unsigned char)line[n]);
}

static int cob_comment(const char *line)
{
    const char *p;

    if (line == NULL) return 1;
    p = line;
    while (*p == ' ' || *p == '\t') ++p;
    if (p[0] == '*' && p[1] == '>') return 1;
    if (strlen(line) > 6U && line[6] == '*') return 1;
    return 0;
}

static void cob_add_name(char names[][COB_NAME_MAX],
                         unsigned int counts[],
                         unsigned int *used,
                         const char *name)
{
    unsigned int i;

    if (name == NULL || *name == '\0' || used == NULL) return;
    for (i = 0U; i < *used; ++i) {
        if (cob_eq_ci(names[i], name)) {
            ++counts[i];
            return;
        }
    }
    if (*used >= COB_NAME_COUNT) return;
    (void)strncpy(names[*used], name, COB_NAME_MAX - 1U);
    names[*used][COB_NAME_MAX - 1U] = '\0';
    counts[*used] = 1U;
    ++*used;
}

static int cob_reserved_label(const char *name)
{
    static const char *reserved[] = {
        "IDENTIFICATION", "ENVIRONMENT", "DATA", "PROCEDURE",
        "WORKING-STORAGE", "LOCAL-STORAGE", "LINKAGE", "FILE",
        "IF", "ELSE", "END-IF", "EVALUATE", "END-EVALUATE",
        "SEARCH", "END-SEARCH", "PERFORM", "END-PERFORM",
        "STOP", "GOBACK", "EXIT"
    };
    unsigned int i;

    for (i = 0U; i < (unsigned int)(sizeof(reserved) / sizeof(reserved[0])); ++i) {
        if (cob_eq_ci(name, reserved[i])) return 1;
    }
    return 0;
}

static void cob_scan_label(const char *line, cob_scope_count *out)
{
    char first[COB_NAME_MAX];
    char second[16];
    char extra[8];
    int fields;
    size_t len;

    if (line == NULL || out == NULL || cob_comment(line)) return;
    first[0] = '\0';
    second[0] = '\0';
    extra[0] = '\0';
    fields = sscanf(line, " %31[A-Za-z0-9_-] %15[A-Za-z-] %7s",
                    first, second, extra);
    if (fields <= 0 || cob_reserved_label(first)) return;

    len = strlen(first);
    if (fields >= 2 && cob_eq_ci(second, "SECTION.")) {
        cob_add_name(out->section, out->section_count,
                     &out->section_used, first);
        return;
    }

    if (fields == 1) {
        const char *p;
        p = line;
        while (*p == ' ' || *p == '\t') ++p;
        p += len;
        if (*p == '.') {
            ++p;
            while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') ++p;
            if (*p == '\0') {
                cob_add_name(out->para, out->para_count,
                             &out->para_used, first);
            }
        }
    }
}

static void cob_scan(const char *text, cob_scope_count *out)
{
    const char *p;
    const char *end;
    char line[1024];
    size_t n;

    (void)memset(out, 0, sizeof(*out));
    if (text == NULL) return;
    p = text;
    while (*p != '\0') {
        end = strchr(p, '\n');
        n = end == NULL ? strlen(p) : (size_t)(end - p) + 1U;
        if (n >= sizeof(line)) n = sizeof(line) - 1U;
        (void)memcpy(line, p, n);
        line[n] = '\0';
        if (!cob_comment(line)) {
            if (cob_token_at(line, "IF")) ++out->if_open;
            if (cob_token_at(line, "END-IF")) ++out->if_close;
            if (cob_token_at(line, "EVALUATE")) ++out->eval_open;
            if (cob_token_at(line, "END-EVALUATE")) ++out->eval_close;
            if (cob_token_at(line, "SEARCH")) ++out->search_open;
            if (cob_token_at(line, "END-SEARCH")) ++out->search_close;
            cob_scan_label(line, out);
        }
        if (end == NULL) break;
        p = end + 1;
    }
}

static unsigned int cob_name_count(char names[][COB_NAME_MAX],
                                   unsigned int counts[],
                                   unsigned int used,
                                   const char *name)
{
    unsigned int i;

    for (i = 0U; i < used; ++i) {
        if (cob_eq_ci(names[i], name)) return counts[i];
    }
    return 0U;
}

static int cob_dup_names(cob_scope_count *before,
                         cob_scope_count *after,
                         int sections)
{
    unsigned int i;
    char (*names)[COB_NAME_MAX];
    unsigned int *counts;
    unsigned int used;
    unsigned int prior;

    names = sections ? after->section : after->para;
    counts = sections ? after->section_count : after->para_count;
    used = sections ? after->section_used : after->para_used;
    for (i = 0U; i < used; ++i) {
        prior = sections ?
            cob_name_count(before->section, before->section_count,
                           before->section_used, names[i]) :
            cob_name_count(before->para, before->para_count,
                           before->para_used, names[i]);
        if (prior > 0U && counts[i] > prior) return 1;
    }
    return 0;
}

static int cob_pair_safe(long bo, long bc, long ao, long ac)
{
    long dopen;
    long dclose;

    dopen = ao - bo;
    dclose = ac - bc;
    if (dopen != dclose) return 0;
    if (ac > ao) return 0;
    return 1;
}

int cobol_edit_safe(const char *path,
                    const char *original,
                    const char *candidate,
                    char *reason,
                    size_t reason_size)
{
    cob_scope_count before;
    cob_scope_count after;

    cob_reason(reason, reason_size, "");
    if (!cob_ext(path)) return 1;
    if (original == NULL || candidate == NULL) {
        cob_reason(reason, reason_size,
                   "COBOL structural validation lacks complete source text.");
        return 0;
    }

    cob_scan(original, &before);
    cob_scan(candidate, &after);

    if (!cob_pair_safe(before.if_open, before.if_close,
                       after.if_open, after.if_close)) {
        cob_reason(reason, reason_size,
                   "COBOL edit changes IF scope without matching END-IF structure.");
        return 0;
    }
    if (!cob_pair_safe(before.eval_open, before.eval_close,
                       after.eval_open, after.eval_close)) {
        cob_reason(reason, reason_size,
                   "COBOL edit changes EVALUATE scope without matching END-EVALUATE structure.");
        return 0;
    }
    if (!cob_pair_safe(before.search_open, before.search_close,
                       after.search_open, after.search_close)) {
        cob_reason(reason, reason_size,
                   "COBOL edit changes SEARCH scope without matching END-SEARCH structure.");
        return 0;
    }
    if (cob_dup_names(&before, &after, 0)) {
        cob_reason(reason, reason_size,
                   "COBOL edit duplicates an existing paragraph label.");
        return 0;
    }
    if (cob_dup_names(&before, &after, 1)) {
        cob_reason(reason, reason_size,
                   "COBOL edit duplicates an existing SECTION label.");
        return 0;
    }

    return 1;
}
