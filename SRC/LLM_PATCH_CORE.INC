#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "llm_internal.h"

#define PATCH_MAX_HUNKS 16U
#define PATCH_PATH_MAX 256U
#define PATCH_HUNK_TEXT_MAX 32768U
#define PATCH_FILE_MAX 1048576U
#define PATCH_LAST_MAX 2048U

typedef struct patch_hunk {
    char *old_text;
    char *new_text;
    size_t offset;
} patch_hunk;

typedef struct patch_doc {
    char target[PATCH_PATH_MAX];
    patch_hunk hunks[PATCH_MAX_HUNKS];
    unsigned int count;
} patch_doc;

static char patch_last[PATCH_LAST_MAX] =
    "No structured patch has been processed.";

static int eq_ci(const char *a, const char *b)
{
    unsigned char x, y;
    if (a == NULL || b == NULL) return 0;
    while (*a != '\0' && *b != '\0') {
        x = (unsigned char)*a++;
        y = (unsigned char)*b++;
        if (x >= 'a' && x <= 'z') x = (unsigned char)(x - 'a' + 'A');
        if (y >= 'a' && y <= 'z') y = (unsigned char)(y - 'a' + 'A');
        if (x != y) return 0;
    }
    return *a == '\0' && *b == '\0';
}

static int true_value(const char *v)
{
    return eq_ci(v, "YES") || eq_ci(v, "TRUE") ||
           eq_ci(v, "1") || eq_ci(v, "ON");
}

static int policy_ok(void)
{
    const char *v = getenv("OVMS_AGENT_APPROVAL_POLICY");
    return eq_ci(v, "WORKSPACE") || eq_ci(v, "WRITE") || eq_ci(v, "FULL");
}

static int safe_target(const char *p)
{
    if (p == NULL || *p == '\0' || strlen(p) >= PATCH_PATH_MAX) return 0;
    if (p[0] == '/' || strstr(p, "..") != NULL ||
        strchr(p, ':') != NULL || strchr(p, '[') != NULL ||
        strchr(p, ']') != NULL || strchr(p, '\\') != NULL) return 0;
    return 1;
}

static void free_doc(patch_doc *d)
{
    unsigned int i;
    if (d == NULL) return;
    for (i = 0U; i < d->count; ++i) {
        free(d->hunks[i].old_text);
        free(d->hunks[i].new_text);
    }
    (void)memset(d, 0, sizeof(*d));
}

static void trim_marker(char *s)
{
    size_t n;
    if (s == NULL) return;
    n = strlen(s);
    while (n > 0U && (s[n-1U] == '\n' || s[n-1U] == '\r')) s[--n] = '\0';
}

static int append_text(char **dst, size_t *used, const char *line)
{
    size_t n;
    char *p;
    n = strlen(line);
    if (*used + n >= PATCH_HUNK_TEXT_MAX) return 0;
    p = (char *)realloc(*dst, *used + n + 1U);
    if (p == NULL) return 0;
    *dst = p;
    (void)memcpy(p + *used, line, n);
    *used += n;
    p[*used] = '\0';
    return 1;
}

static int parse_spec(const char *path, patch_doc *d,
                      char *err, size_t errsz)
{
    FILE *fp;
    char line[1024], mark[1024];
    int state = 0;
    char *oldp = NULL, *newp = NULL;
    size_t oldu = 0U, newu = 0U;

    (void)memset(d, 0, sizeof(*d));
    fp = fopen(path, "r");
    if (fp == NULL) {
        (void)snprintf(err, errsz, "unable to open patch spec");
        return 0;
    }

    while (fgets(line, sizeof(line), fp) != NULL) {
        (void)strncpy(mark, line, sizeof(mark)-1U);
        mark[sizeof(mark)-1U] = '\0';
        trim_marker(mark);

        if (state == 0) {
            if (strncmp(mark, "TARGET ", 7U) != 0 || !safe_target(mark+7U)) {
                (void)snprintf(err, errsz, "invalid TARGET line");
                goto bad;
            }
            (void)strncpy(d->target, mark+7U, sizeof(d->target)-1U);
            d->target[sizeof(d->target)-1U] = '\0';
            state = 1;
        } else if (state == 1) {
            if (strcmp(mark, "@@OLD") != 0) {
                (void)snprintf(err, errsz, "expected @@OLD");
                goto bad;
            }
            oldp = newp = NULL; oldu = newu = 0U; state = 2;
        } else if (state == 2) {
            if (strcmp(mark, "@@NEW") == 0) {
                if (oldu == 0U) {
                    (void)snprintf(err, errsz, "OLD text may not be empty");
                    goto bad;
                }
                state = 3;
            } else if (!append_text(&oldp, &oldu, line)) {
                (void)snprintf(err, errsz, "OLD text too large");
                goto bad;
            }
        } else {
            if (strcmp(mark, "@@END") == 0) {
                if (d->count >= PATCH_MAX_HUNKS) {
                    (void)snprintf(err, errsz, "too many hunks");
                    goto bad;
                }
                if (newp == NULL) {
                    newp = (char *)malloc(1U);
                    if (newp == NULL) goto bad;
                    newp[0] = '\0';
                }
                d->hunks[d->count].old_text = oldp;
                d->hunks[d->count].new_text = newp;
                ++d->count;
                oldp = newp = NULL; state = 1;
            } else if (!append_text(&newp, &newu, line)) {
                (void)snprintf(err, errsz, "NEW text too large");
                goto bad;
            }
        }
    }

    (void)fclose(fp);
    if (state != 1 || d->count == 0U) {
        (void)snprintf(err, errsz, "incomplete or empty patch spec");
        free(oldp); free(newp); free_doc(d); return 0;
    }
    return 1;

bad:
    (void)fclose(fp);
    free(oldp); free(newp); free_doc(d);
    return 0;
}

static char *read_all(const char *path, size_t *sz)
{
    FILE *fp;
    long n;
    size_t got;
    char *p;
    fp = fopen(path, "r");
    if (fp == NULL) return NULL;
    if (fseek(fp, 0L, SEEK_END) != 0 || (n = ftell(fp)) < 0L ||
        n >= (long)PATCH_FILE_MAX || fseek(fp, 0L, SEEK_SET) != 0) {
        (void)fclose(fp); return NULL;
    }
    p = (char *)malloc((size_t)n + 1U);
    if (p == NULL) { (void)fclose(fp); return NULL; }
    got = fread(p, 1U, (size_t)n, fp);
    if (got != (size_t)n && ferror(fp)) {
        free(p); (void)fclose(fp); return NULL;
    }
    p[got] = '\0'; (void)fclose(fp); *sz = got; return p;
}

static unsigned int match_count(const char *text, const char *needle,
                                size_t *offset)
{
    const char *p = text, *q;
    unsigned int count = 0U;
    while ((q = strstr(p, needle)) != NULL) {
        if (count == 0U) *offset = (size_t)(q - text);
        ++count;
        if (count > 1U) break;
        p = q + 1;
    }
    return count;
}

static void sort_hunks(patch_doc *d)
{
    unsigned int i, j;
    patch_hunk t;
    for (i = 0U; i < d->count; ++i)
        for (j = i + 1U; j < d->count; ++j)
            if (d->hunks[j].offset < d->hunks[i].offset) {
                t = d->hunks[i]; d->hunks[i] = d->hunks[j]; d->hunks[j] = t;
            }
}

static int validate_hunks(patch_doc *d, const char *original,
                          char *err, size_t errsz)
{
    unsigned int i, n;
    size_t end = 0U;
    for (i = 0U; i < d->count; ++i) {
        n = match_count(original, d->hunks[i].old_text, &d->hunks[i].offset);
        if (n == 0U) {
            (void)snprintf(err, errsz, "hunk %u OLD text not found", i+1U);
            return 0;
        }
        if (n != 1U) {
            (void)snprintf(err, errsz, "hunk %u OLD text is ambiguous", i+1U);
            return 0;
        }
    }
    sort_hunks(d);
    for (i = 0U; i < d->count; ++i) {
        if (i > 0U && d->hunks[i].offset < end) {
            (void)snprintf(err, errsz, "hunks overlap");
            return 0;
        }
        end = d->hunks[i].offset + strlen(d->hunks[i].old_text);
    }
    return 1;
}

static char *render_patch(const patch_doc *d, const char *orig,
                          size_t origsz, size_t *outsz)
{
    unsigned int i;
    size_t total = origsz, cursor = 0U, used = 0U, a, b, prefix;
    char *out;
    for (i = 0U; i < d->count; ++i) {
        a = strlen(d->hunks[i].old_text);
        b = strlen(d->hunks[i].new_text);
        total = b >= a ? total + (b-a) : total - (a-b);
    }
    if (total >= PATCH_FILE_MAX) return NULL;
    out = (char *)malloc(total + 1U);
    if (out == NULL) return NULL;
    for (i = 0U; i < d->count; ++i) {
        prefix = d->hunks[i].offset - cursor;
        (void)memcpy(out+used, orig+cursor, prefix); used += prefix;
        b = strlen(d->hunks[i].new_text);
        (void)memcpy(out+used, d->hunks[i].new_text, b); used += b;
        cursor = d->hunks[i].offset + strlen(d->hunks[i].old_text);
    }
    (void)memcpy(out+used, orig+cursor, origsz-cursor); used += origsz-cursor;
    out[used] = '\0'; *outsz = used; return out;
}

static int write_all(const char *path, const char *text, size_t sz)
{
    FILE *fp = fopen(path, "w");
    size_t n;
    if (fp == NULL) return 0;
    n = fwrite(text, 1U, sz, fp);
    if (n != sz) { (void)fclose(fp); return 0; }
    return fclose(fp) == 0;
}

static int process_patch(const char *spec, int apply,
                         char *out, size_t outsz)
{
    patch_doc d;
    char err[256];
    char *orig, *result;
    size_t origsz, resultsz;
    int ok = 1;

    if (!parse_spec(spec, &d, err, sizeof(err))) {
        (void)snprintf(out, outsz, "Structured patch rejected: %s.", err);
        return 0;
    }
    orig = read_all(d.target, &origsz);
    if (orig == NULL) {
        (void)snprintf(out, outsz, "Structured patch rejected: unable to read %s.", d.target);
        free_doc(&d); return 0;
    }
    if (!validate_hunks(&d, orig, err, sizeof(err))) {
        (void)snprintf(out, outsz, "Structured patch rejected: %s.", err);
        free(orig); free_doc(&d); return 0;
    }
    result = render_patch(&d, orig, origsz, &resultsz);
    if (result == NULL) {
        (void)snprintf(out, outsz, "Structured patch rejected: render failed.");
        free(orig); free_doc(&d); return 0;
    }

    if (apply) {
        if (!true_value(getenv("OVMS_AGENT_WRITE_ENABLED"))) {
            (void)snprintf(out, outsz, "Structured patch refused: OVMS_AGENT_WRITE_ENABLED is not enabled.");
            ok = 0;
        } else if (!policy_ok()) {
            (void)snprintf(out, outsz, "Structured patch refused: approval policy must be workspace or full.");
            ok = 0;
        } else if (!write_all(d.target, result, resultsz)) {
            (void)snprintf(out, outsz, "Structured patch failed writing %s.", d.target);
            ok = 0;
        } else {
            (void)snprintf(out, outsz,
                "Structured patch applied: %s (%u hunks, %lu -> %lu bytes).",
                d.target, d.count, (unsigned long)origsz, (unsigned long)resultsz);
        }
    } else {
        (void)snprintf(out, outsz,
            "Structured patch valid: %s (%u hunks, %lu -> %lu bytes).",
            d.target, d.count, (unsigned long)origsz, (unsigned long)resultsz);
    }

    free(result); free(orig); free_doc(&d); return ok;
}


static char *patch_json_string(const char *json, const char *name)
{
    char key[96];
    const char *p;
    char *out;
    char *q;
    size_t cap;

    if (json == NULL || name == NULL) return NULL;

    if (snprintf(key, sizeof(key), "\"%s\"", name) < 0) return NULL;
    p = strstr(json, key);
    if (p == NULL) return NULL;
    p += strlen(key);

    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') ++p;
    if (*p != ':') return NULL;
    ++p;
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') ++p;
    if (*p != '"') return NULL;
    ++p;

    cap = strlen(p) + 1U;
    out = (char *)malloc(cap);
    if (out == NULL) return NULL;
    q = out;

    while (*p != '\0' && *p != '"') {
        if (*p == '\\') {
            ++p;
            if (*p == '\0') { free(out); return NULL; }

            switch (*p) {
            case '"':  *q++ = '"';  break;
            case '\\': *q++ = '\\'; break;
            case '/':  *q++ = '/';  break;
            case 'b':  *q++ = '\b'; break;
            case 'f':  *q++ = '\f'; break;
            case 'n':  *q++ = '\n'; break;
            case 'r':  *q++ = '\r'; break;
            case 't':  *q++ = '\t'; break;
            default:
                free(out);
                return NULL;
            }
            ++p;
        } else {
            *q++ = *p++;
        }
    }

    if (*p != '"') {
        free(out);
        return NULL;
    }

    *q = '\0';
    return out;
}

int llm_patch_apply_json(const char *arguments,
                            char *output,
                            size_t output_size)
{
    static const char temp_spec[] = "OVMS_AGENT_AUTO_PATCH.TMP";
    char *path;
    char *body;
    FILE *file;
    int result;
    size_t body_length;

    if (output == NULL || output_size == 0U) return 0;

    path = patch_json_string(arguments, "path");
    body = patch_json_string(arguments, "patch");

    if (path == NULL || body == NULL) {
        free(path);
        free(body);
        (void)snprintf(
            output, output_size,
            "Structured patch rejected: path and patch are required."
        );
        return 0;
    }

    if (!safe_target(path)) {
        free(path);
        free(body);
        (void)snprintf(
            output, output_size,
            "Structured patch rejected: unsafe or invalid target."
        );
        return 0;
    }

    while (remove(temp_spec) == 0) {
    }

    file = fopen(temp_spec, "w");
    if (file == NULL) {
        free(path);
        free(body);
        (void)snprintf(
            output, output_size,
            "Structured patch rejected: unable to create temporary spec."
        );
        return 0;
    }

    body_length = strlen(body);

    if (fprintf(file, "TARGET %s\n", path) < 0 ||
        fputs(body, file) == EOF ||
        (body_length > 0U && body[body_length - 1U] != '\n' &&
         fputc('\n', file) == EOF) ||
        fclose(file) != 0) {
        (void)remove(temp_spec);
        free(path);
        free(body);
        (void)snprintf(
            output, output_size,
            "Structured patch rejected: unable to write temporary spec."
        );
        return 0;
    }

    result = llm_patch_apply(temp_spec, output, output_size);

    while (remove(temp_spec) == 0) {
    }

    free(path);
    free(body);
    return result;
}

int llm_patch_validate(const char *p, char *o, size_t n)
{ return process_patch(p, 0, o, n); }
int llm_patch_dry(const char *p, char *o, size_t n)
{ return process_patch(p, 0, o, n); }
int llm_patch_apply(const char *p, char *o, size_t n)
{ return process_patch(p, 1, o, n); }

int llm_patch_last_text(char *o, size_t n)
{
    int w;
    if (o == NULL || n == 0U) return 0;
    w = snprintf(o, n, "%s\n", patch_last);
    return w >= 0 && (size_t)w < n;
}

static void run_patch_cmd(const char *arg, int mode)
{
    char out[PATCH_LAST_MAX];
    int ok;
    if (arg == NULL || *arg == '\0') {
        (void)puts(mode == 2 ? "Usage: AGENT/PATCH patch-spec-filespec" :
                   mode == 1 ? "Usage: AGENT/PATCH/DRY patch-spec-filespec" :
                               "Usage: AGENT/PATCH/VALIDATE patch-spec-filespec");
        return;
    }
    ok = mode == 2 ? llm_patch_apply(arg, out, sizeof(out)) :
         mode == 1 ? llm_patch_dry(arg, out, sizeof(out)) :
                     llm_patch_validate(arg, out, sizeof(out));
    (void)strncpy(patch_last, out, sizeof(patch_last)-1U);
    patch_last[sizeof(patch_last)-1U] = '\0';
    (void)puts(out);
    if (!ok && mode == 2) (void)puts("No validated multi-hunk change was committed.");
}

void llm_patch_validate_cmd(const char *a) { run_patch_cmd(a, 0); }
void llm_patch_dry_cmd(const char *a) { run_patch_cmd(a, 1); }
void llm_patch_apply_cmd(const char *a) { run_patch_cmd(a, 2); }
void llm_patch_last_cmd(void)
{
    char out[PATCH_LAST_MAX+16U];
    if (llm_patch_last_text(out, sizeof(out))) (void)fputs(out, stdout);
}
