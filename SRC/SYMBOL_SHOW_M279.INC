/*
 * M279 command-side SHOW/STRUCT classifier.
 *
 * The legacy symbol scanner can confuse `struct tag variable;` with the
 * beginning of the tag definition.  Keep this command-local and conservative:
 * a tag is a definition only when the next significant token after the tag is
 * an opening brace, on the same line or the next nonblank source line.
 */

#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <string.h>

#define M279_TYPE_PATH_MAX 512
#define M279_TYPE_LINE_MAX 2048

static int m279_id_char(int ch)
{
    return isalnum((unsigned char)ch) || ch == '_';
}

static void m279_clean_type_line(char *line, int *in_block)
{
    char *p;
    int in_string;
    int in_char;
    int escaped;

    in_string = 0;
    in_char = 0;
    escaped = 0;

    for (p = line; *p != '\0'; ++p) {
        if (*in_block) {
            if (p[0] == '*' && p[1] == '/') {
                p[0] = ' ';
                p[1] = ' ';
                ++p;
                *in_block = 0;
            } else {
                *p = ' ';
            }
            continue;
        }

        if (in_string) {
            if (escaped) {
                escaped = 0;
            } else if (*p == '\\') {
                escaped = 1;
            } else if (*p == '"') {
                in_string = 0;
            }
            *p = ' ';
            continue;
        }

        if (in_char) {
            if (escaped) {
                escaped = 0;
            } else if (*p == '\\') {
                escaped = 1;
            } else if (*p == '\'') {
                in_char = 0;
            }
            *p = ' ';
            continue;
        }

        if (p[0] == '/' && p[1] == '*') {
            p[0] = ' ';
            p[1] = ' ';
            ++p;
            *in_block = 1;
            continue;
        }

        if (p[0] == '/' && p[1] == '/') {
            while (*p != '\0') {
                *p++ = ' ';
            }
            break;
        }

        if (*p == '"') {
            in_string = 1;
            *p = ' ';
        } else if (*p == '\'') {
            in_char = 1;
            *p = ' ';
        }
    }
}

static const char *m279_skip_ws(const char *p)
{
    while (p != NULL && *p != '\0' &&
           isspace((unsigned char)*p)) {
        ++p;
    }
    return p;
}

static int m279_tag_candidate(const char *line,
                              const char *symbol,
                              const char **kind_out,
                              int *needs_brace)
{
    static const char *kinds[] = { "struct", "union", "enum", NULL };
    const char **kind;
    const char *p;
    const char *name;
    size_t kind_len;
    size_t name_len;

    if (line == NULL || symbol == NULL || needs_brace == NULL) {
        return 0;
    }

    p = m279_skip_ws(line);
    if (strncmp(p, "typedef", 7U) == 0 &&
        isspace((unsigned char)p[7])) {
        p = m279_skip_ws(p + 7);
    }

    for (kind = kinds; *kind != NULL; ++kind) {
        kind_len = strlen(*kind);
        if (strncmp(p, *kind, kind_len) != 0 ||
            !isspace((unsigned char)p[kind_len])) {
            continue;
        }

        p = m279_skip_ws(p + kind_len);
        name = p;
        while (m279_id_char((unsigned char)*p)) {
            ++p;
        }

        name_len = (size_t)(p - name);
        if (name_len != strlen(symbol) ||
            strncmp(name, symbol, name_len) != 0) {
            continue;
        }

        p = m279_skip_ws(p);
        if (*p == '{') {
            *needs_brace = 0;
        } else if (*p == '\0') {
            *needs_brace = 1;
        } else {
            return 0;
        }

        if (kind_out != NULL) {
            *kind_out = *kind;
        }
        return 1;
    }

    return 0;
}

static int m279_find_type(const char *symbol,
                          char *path_out,
                          size_t path_size,
                          unsigned long *line_out,
                          char *kind_out,
                          size_t kind_size)
{
    const char *directory_name;
    DIR *directory;
    struct dirent *entry;

    directory_name = "SRC";
    directory = opendir(directory_name);
    if (directory == NULL) {
        directory_name = ".";
        directory = opendir(directory_name);
    }
    if (directory == NULL) {
        return 0;
    }

    while ((entry = readdir(directory)) != NULL) {
        FILE *file;
        char path[M279_TYPE_PATH_MAX];
        char line[M279_TYPE_LINE_MAX];
        char clean[M279_TYPE_LINE_MAX];
        unsigned long line_number;
        int in_block;
        int pending;
        unsigned long pending_line;
        char pending_kind[16];
        size_t len;

        len = strlen(entry->d_name);
        if (len < 2U || entry->d_name[len - 2U] != '.' ||
            (tolower((unsigned char)entry->d_name[len - 1U]) != 'c' &&
             tolower((unsigned char)entry->d_name[len - 1U]) != 'h')) {
            continue;
        }

        if (strcmp(directory_name, ".") == 0) {
            if (snprintf(path, sizeof(path), "%s", entry->d_name) < 0) {
                continue;
            }
        } else {
            if (snprintf(path, sizeof(path), "%s/%s",
                         directory_name, entry->d_name) < 0) {
                continue;
            }
        }
        if (strlen(path) >= sizeof(path)) {
            continue;
        }

        file = fopen(path, "r");
        if (file == NULL) {
            continue;
        }

        line_number = 0UL;
        in_block = 0;
        pending = 0;
        pending_line = 0UL;
        pending_kind[0] = '\0';

        while (fgets(line, sizeof(line), file) != NULL) {
            const char *p;
            const char *kind;
            int needs_brace;

            ++line_number;
            (void)strncpy(clean, line, sizeof(clean) - 1U);
            clean[sizeof(clean) - 1U] = '\0';
            m279_clean_type_line(clean, &in_block);
            p = m279_skip_ws(clean);

            if (pending) {
                if (*p == '\0') {
                    continue;
                }
                if (*p == '{') {
                    if (strlen(path) < path_size &&
                        strlen(pending_kind) < kind_size) {
                        (void)strcpy(path_out, path);
                        (void)strcpy(kind_out, pending_kind);
                        *line_out = pending_line;
                        (void)fclose(file);
                        (void)closedir(directory);
                        return 1;
                    }
                    pending = 0;
                    break;
                }
                pending = 0;
            }

            kind = NULL;
            needs_brace = 0;
            if (m279_tag_candidate(clean, symbol, &kind, &needs_brace)) {
                if (!needs_brace) {
                    if (strlen(path) < path_size &&
                        strlen(kind) < kind_size) {
                        (void)strcpy(path_out, path);
                        (void)strcpy(kind_out, kind);
                        *line_out = line_number;
                        (void)fclose(file);
                        (void)closedir(directory);
                        return 1;
                    }
                    break;
                }

                pending = 1;
                pending_line = line_number;
                if (strlen(kind) < sizeof(pending_kind)) {
                    (void)strcpy(pending_kind, kind);
                } else {
                    pending = 0;
                }
            }
        }

        (void)fclose(file);
    }

    (void)closedir(directory);
    return 0;
}

static int m279_print_type(const char *path,
                           unsigned long start_line)
{
    FILE *file;
    char line[M279_TYPE_LINE_MAX];
    unsigned long line_number;
    int depth;
    int started;
    int in_block;
    int in_string;
    int in_char;
    int escaped;

    file = fopen(path, "r");
    if (file == NULL) {
        return 0;
    }

    line_number = 0UL;
    depth = 0;
    started = 0;
    in_block = 0;
    in_string = 0;
    in_char = 0;
    escaped = 0;

    while (fgets(line, sizeof(line), file) != NULL) {
        char *p;

        ++line_number;
        if (line_number < start_line) {
            continue;
        }

        (void)printf("%6lu  %s", line_number, line);

        for (p = line; *p != '\0'; ++p) {
            if (in_block) {
                if (p[0] == '*' && p[1] == '/') {
                    ++p;
                    in_block = 0;
                }
                continue;
            }
            if (in_string) {
                if (escaped) escaped = 0;
                else if (*p == '\\') escaped = 1;
                else if (*p == '"') in_string = 0;
                continue;
            }
            if (in_char) {
                if (escaped) escaped = 0;
                else if (*p == '\\') escaped = 1;
                else if (*p == '\'') in_char = 0;
                continue;
            }
            if (p[0] == '/' && p[1] == '*') {
                ++p;
                in_block = 1;
                continue;
            }
            if (p[0] == '/' && p[1] == '/') {
                break;
            }
            if (*p == '"') {
                in_string = 1;
                continue;
            }
            if (*p == '\'') {
                in_char = 1;
                continue;
            }
            if (*p == '{') {
                ++depth;
                started = 1;
            } else if (*p == '}' && started) {
                --depth;
                if (depth == 0 && strchr(p, ';') != NULL) {
                    (void)fclose(file);
                    return 1;
                }
            } else if (*p == ';' && started && depth == 0) {
                (void)fclose(file);
                return 1;
            }
        }
    }

    (void)fclose(file);
    return started;
}

static void m279_show_struct(agent_state *state,
                             const char *symbol)
{
    char path[M279_TYPE_PATH_MAX];
    char kind[16];
    unsigned long line_number;

    if (state == NULL || state->project_root == NULL ||
        *state->project_root == '\0') {
        (void)puts("OVMS_AGENT_ROOT is not defined.");
        return;
    }
    if (symbol == NULL || *symbol == '\0') {
        return;
    }

    path[0] = '\0';
    kind[0] = '\0';
    line_number = 0UL;

    if (!m279_find_type(symbol, path, sizeof(path),
                        &line_number, kind, sizeof(kind))) {
        (void)printf(
            "No struct, union, or enum definition found for %s.\n",
            symbol
        );
        return;
    }

    (void)printf("%s %s defined in %s at line %lu\n",
                 kind, symbol, path, line_number);
    (void)puts("----------------------------------------");

    if (!m279_print_type(path, line_number)) {
        (void)puts("Unable to determine the complete type definition.");
    }
}
