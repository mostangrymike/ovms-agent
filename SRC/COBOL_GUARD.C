#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
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
    long program_end;
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

static int cob_two_tokens(const char *line,
                          const char *first,
                          const char *second)
{
    char a[24];
    char b[24];

    if (line == NULL || first == NULL || second == NULL) return 0;
    a[0] = '\0';
    b[0] = '\0';
    if (sscanf(line, " %23[A-Za-z-] %23[A-Za-z-]", a, b) != 2) {
        return 0;
    }
    return cob_eq_ci(a, first) && cob_eq_ci(b, second);
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
        "STOP", "GOBACK", "EXIT", "END", "PROGRAM"
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
            if (cob_two_tokens(line, "END", "PROGRAM")) {
                ++out->program_end;
            }
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
        if (counts[i] > prior && counts[i] > 1U) return 1;
    }
    return 0;
}

static int cob_missing_names(cob_scope_count *before,
                             cob_scope_count *after,
                             int sections)
{
    unsigned int i;
    char (*names)[COB_NAME_MAX];
    unsigned int *counts;
    unsigned int used;
    unsigned int now;

    names = sections ? before->section : before->para;
    counts = sections ? before->section_count : before->para_count;
    used = sections ? before->section_used : before->para_used;
    for (i = 0U; i < used; ++i) {
        now = sections ?
            cob_name_count(after->section, after->section_count,
                           after->section_used, names[i]) :
            cob_name_count(after->para, after->para_count,
                           after->para_used, names[i]);
        if (now < counts[i]) return 1;
    }
    return 0;
}

static int cob_rename_delta(cob_scope_count *before,
                            cob_scope_count *after,
                            char *old_name,
                            char *new_name)
{
    unsigned int i;
    unsigned int now;
    unsigned int prior;
    int old_found;
    int new_found;

    if (before == NULL || after == NULL ||
        old_name == NULL || new_name == NULL) return 0;

    old_name[0] = '\0';
    new_name[0] = '\0';
    old_found = 0;
    new_found = 0;

    for (i = 0U; i < before->para_used; ++i) {
        now = cob_name_count(after->para, after->para_count,
                             after->para_used, before->para[i]);
        if (now != before->para_count[i]) {
            if (old_found || before->para_count[i] != 1U || now != 0U) {
                return 0;
            }
            (void)strcpy(old_name, before->para[i]);
            old_found = 1;
        }
    }

    for (i = 0U; i < after->para_used; ++i) {
        prior = cob_name_count(before->para, before->para_count,
                               before->para_used, after->para[i]);
        if (prior != after->para_count[i] && prior == 0U) {
            if (new_found || after->para_count[i] != 1U) {
                return 0;
            }
            (void)strcpy(new_name, after->para[i]);
            new_found = 1;
        }
    }

    return old_found && new_found;
}

static int cob_name_match_at(const char *text, const char *name)
{
    size_t i;
    size_t length;

    if (text == NULL || name == NULL || *name == '\0') return 0;
    length = strlen(name);
    for (i = 0U; i < length; ++i) {
        if (text[i] == '\0' ||
            toupper((unsigned char)text[i]) !=
            toupper((unsigned char)name[i])) return 0;
    }
    return !cob_word_char((unsigned char)text[length]);
}

static int cob_append(char **buffer,
                      size_t *used,
                      size_t *capacity,
                      const char *text,
                      size_t length)
{
    char *grown;
    size_t wanted;
    size_t next;

    if (buffer == NULL || used == NULL || capacity == NULL ||
        text == NULL) return 0;

    wanted = *used + length + 1U;
    if (wanted > *capacity) {
        next = *capacity == 0U ? 256U : *capacity;
        while (next < wanted) {
            if (next > (size_t)-1 / 2U) return 0;
            next *= 2U;
        }
        grown = (char *)realloc(*buffer, next);
        if (grown == NULL) return 0;
        *buffer = grown;
        *capacity = next;
    }

    if (length > 0U) {
        (void)memcpy(*buffer + *used, text, length);
        *used += length;
    }
    (*buffer)[*used] = '\0';
    return 1;
}

static int cob_procedure_line(const char *line)
{
    char first[24];
    char second[24];

    if (line == NULL) return 0;
    first[0] = '\0';
    second[0] = '\0';
    if (sscanf(line, " %23[A-Za-z-] %23[A-Za-z-]",
               first, second) != 2) return 0;
    return cob_eq_ci(first, "PROCEDURE") &&
           cob_eq_ci(second, "DIVISION");
}

static int cob_rename_line(const char *line,
                           size_t length,
                           const char *old_name,
                           const char *new_name,
                           char **buffer,
                           size_t *used,
                           size_t *capacity,
                           unsigned int *replacements)
{
    size_t i;
    size_t old_length;
    char quote;

    if (line == NULL || old_name == NULL || new_name == NULL ||
        buffer == NULL || used == NULL || capacity == NULL ||
        replacements == NULL) return 0;

    old_length = strlen(old_name);
    quote = '\0';
    i = 0U;
    while (i < length) {
        if (quote != '\0') {
            if (!cob_append(buffer, used, capacity, line + i, 1U)) return 0;
            if (line[i] == quote) {
                if (i + 1U < length && line[i + 1U] == quote) {
                    if (!cob_append(buffer, used, capacity,
                                    line + i + 1U, 1U)) return 0;
                    i += 2U;
                    continue;
                }
                quote = '\0';
            }
            ++i;
            continue;
        }

        if (line[i] == '\'' || line[i] == '"') {
            quote = line[i];
            if (!cob_append(buffer, used, capacity, line + i, 1U)) return 0;
            ++i;
            continue;
        }

        if ((i == 0U || !cob_word_char((unsigned char)line[i - 1U])) &&
            cob_name_match_at(line + i, old_name)) {
            if (!cob_append(buffer, used, capacity,
                            new_name, strlen(new_name))) return 0;
            i += old_length;
            ++*replacements;
            continue;
        }

        if (!cob_append(buffer, used, capacity, line + i, 1U)) return 0;
        ++i;
    }
    return 1;
}

static char *cob_render_rename(const char *original,
                               const char *old_name,
                               const char *new_name,
                               unsigned int *replacements)
{
    const char *cursor;
    const char *end;
    char line[1024];
    size_t length;
    size_t used;
    size_t capacity;
    char *buffer;
    int in_procedure;

    if (original == NULL || old_name == NULL || new_name == NULL ||
        replacements == NULL) return NULL;

    *replacements = 0U;
    buffer = NULL;
    used = 0U;
    capacity = 0U;
    in_procedure = 0;
    cursor = original;

    while (*cursor != '\0') {
        end = strchr(cursor, '\n');
        length = end == NULL ? strlen(cursor) :
                 (size_t)(end - cursor) + 1U;
        if (length >= sizeof(line)) {
            free(buffer);
            return NULL;
        }
        (void)memcpy(line, cursor, length);
        line[length] = '\0';

        if (!in_procedure || cob_comment(line)) {
            if (!cob_append(&buffer, &used, &capacity,
                            cursor, length)) {
                free(buffer);
                return NULL;
            }
            if (!in_procedure && cob_procedure_line(line)) {
                in_procedure = 1;
            }
        } else if (!cob_rename_line(
                       cursor, length, old_name, new_name,
                       &buffer, &used, &capacity, replacements)) {
            free(buffer);
            return NULL;
        }

        if (end == NULL) break;
        cursor = end + 1;
    }

    if (buffer == NULL) {
        buffer = (char *)malloc(1U);
        if (buffer != NULL) buffer[0] = '\0';
    }
    return buffer;
}

static int cob_paragraph_rename_safe(const char *original,
                                     const char *candidate,
                                     cob_scope_count *before,
                                     cob_scope_count *after)
{
    char old_name[COB_NAME_MAX];
    char new_name[COB_NAME_MAX];
    char *expected;
    unsigned int replacements;
    int safe;

    if (!cob_rename_delta(before, after, old_name, new_name)) return 0;

    expected = cob_render_rename(
        original, old_name, new_name, &replacements);
    if (expected == NULL) return 0;

    safe = replacements > 0U && strcmp(expected, candidate) == 0;
    free(expected);
    return safe;
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
    if (after.program_end != before.program_end) {
        cob_reason(reason, reason_size,
                   "COBOL edit changes END PROGRAM structure.");
        return 0;
    }
    if (cob_dup_names(&before, &after, 0)) {
        cob_reason(reason, reason_size,
                   "COBOL edit duplicates a paragraph label.");
        return 0;
    }
    if (cob_dup_names(&before, &after, 1)) {
        cob_reason(reason, reason_size,
                   "COBOL edit duplicates a SECTION label.");
        return 0;
    }
    if (cob_missing_names(&before, &after, 0) &&
        !cob_paragraph_rename_safe(
            original, candidate, &before, &after)) {
        cob_reason(reason, reason_size,
                   "COBOL edit removes or inconsistently renames an existing paragraph boundary.");
        return 0;
    }
    if (cob_missing_names(&before, &after, 1)) {
        cob_reason(reason, reason_size,
                   "COBOL edit removes or renames an existing SECTION boundary.");
        return 0;
    }

    return 1;
}

int cobol_text_safe(const char *path,
                    const char *original,
                    const char *old_text,
                    const char *new_text,
                    char *reason,
                    size_t reason_size)
{
    const char *match;
    const char *second;
    char *candidate;
    size_t prefix;
    size_t old_len;
    size_t new_len;
    size_t original_len;
    size_t candidate_len;
    int safe;

    cob_reason(reason, reason_size, "");
    if (!cob_ext(path)) return 1;
    if (original == NULL || old_text == NULL || new_text == NULL ||
        *old_text == '\0') {
        cob_reason(reason, reason_size,
                   "COBOL exact-text validation lacks valid edit text.");
        return 0;
    }

    match = strstr(original, old_text);
    if (match == NULL) {
        cob_reason(reason, reason_size,
                   "COBOL exact-text validation could not find old_text.");
        return 0;
    }
    second = strstr(match + strlen(old_text), old_text);
    if (second != NULL) {
        cob_reason(reason, reason_size,
                   "COBOL exact-text validation requires unique old_text.");
        return 0;
    }

    prefix = (size_t)(match - original);
    old_len = strlen(old_text);
    new_len = strlen(new_text);
    original_len = strlen(original);
    candidate_len = prefix + new_len +
        (original_len - prefix - old_len);
    candidate = (char *)malloc(candidate_len + 1U);
    if (candidate == NULL) {
        cob_reason(reason, reason_size,
                   "COBOL exact-text validation could not allocate candidate.");
        return 0;
    }

    if (prefix > 0U) {
        (void)memcpy(candidate, original, prefix);
    }
    if (new_len > 0U) {
        (void)memcpy(candidate + prefix, new_text, new_len);
    }
    (void)memcpy(candidate + prefix + new_len,
                 match + old_len,
                 original_len - prefix - old_len);
    candidate[candidate_len] = '\0';

    safe = cobol_edit_safe(path, original, candidate,
                           reason, reason_size);
    free(candidate);
    return safe;
}
