#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include "symbol_index.h"

#define SYMBOL_LINE_SIZE 2048
#define SYMBOL_PATH_SIZE 512
#define SYMBOL_NAME_SIZE 256
#define SYMBOL_INDEX_VERSION 2
#define SYMBOL_MANIFEST_PREFIX "F|"

typedef struct symbol_manifest_record {
    char path[SYMBOL_PATH_SIZE];
    unsigned long size;
    unsigned long mtime;
} symbol_manifest_record;

static int symbol_is_identifier_char(int ch)
{
    return isalnum((unsigned char)ch) || ch == '_';
}

static int symbol_name_valid(const char *symbol)
{
    const unsigned char *position;

    if (symbol == NULL ||
        *symbol == '\0' ||
        !(isalpha((unsigned char)*symbol) || *symbol == '_')) {
        return 0;
    }

    for (position = (const unsigned char *)symbol + 1;
         *position != '\0';
         ++position) {
        if (!symbol_is_identifier_char(*position)) {
            return 0;
        }
    }

    return 1;
}

static int symbol_has_c_extension(const char *name)
{
    size_t length;

    if (name == NULL) {
        return 0;
    }

    length = strlen(name);

    return length >= 2U &&
           name[length - 2U] == '.' &&
           (name[length - 1U] == 'C' ||
            name[length - 1U] == 'c');
}

static int symbol_is_keyword(const char *symbol)
{
    static const char *keywords[] = {
        "if", "else", "for", "while", "do", "switch", "case",
        "return", "sizeof", "typedef", "struct", "union", "enum",
        "_Bool", "_Generic", "_Alignof", NULL
    };
    const char **keyword;

    for (keyword = keywords; *keyword != NULL; ++keyword) {
        if (strcmp(symbol, *keyword) == 0) {
            return 1;
        }
    }

    return 0;
}

static int symbol_match_at(const char *line,
                           const char *position,
                           const char *symbol)
{
    size_t length;
    int before;
    int after;

    length = strlen(symbol);
    before = position == line ? 0 : (unsigned char)position[-1];
    after = (unsigned char)position[length];

    return strncmp(position, symbol, length) == 0 &&
           !symbol_is_identifier_char(before) &&
           !symbol_is_identifier_char(after);
}

static int symbol_line_is_definition(const char *line,
                                     const char *symbol)
{
    const char *position;
    const char *after;
    const char *before;
    const char *open_paren;
    const char *semicolon;
    const char *equal_sign;

    position = strstr(line, symbol);

    while (position != NULL) {
        if (symbol_match_at(line, position, symbol)) {
            before = position;

            while (before > line &&
                   isspace((unsigned char)before[-1])) {
                --before;
            }

            after = position + strlen(symbol);

            while (*after != '\0' &&
                   isspace((unsigned char)*after)) {
                ++after;
            }

            if (*after == '(') {
                open_paren = after;
                semicolon = strchr(open_paren, ';');
                equal_sign = strchr(line, '=');

                /*
                 * A function definition must have declaration text before
                 * the symbol on the same source line. Calls at the beginning
                 * of a continuation line, such as:
                 *
                 *     openai_tool_find("name") == NULL,
                 *
                 * are not definitions.
                 */
                if (before > line &&
                    (equal_sign == NULL ||
                     equal_sign > position) &&
                    semicolon == NULL) {
                    return 1;
                }
            }
        }

        position = strstr(position + 1, symbol);
    }

    return 0;
}

static int symbol_line_is_call(const char *line,
                               const char *symbol)
{
    const char *position;
    const char *after;

    position = strstr(line, symbol);

    while (position != NULL) {
        if (symbol_match_at(line, position, symbol)) {
            after = position + strlen(symbol);

            while (*after != '\0' &&
                   isspace((unsigned char)*after)) {
                ++after;
            }

            if (*after == '(' &&
                !symbol_line_is_definition(line, symbol)) {
                return 1;
            }
        }

        position = strstr(position + 1, symbol);
    }

    return 0;
}

static void symbol_clean_line(char *line,
                              int *in_block_comment)
{
    char *position;
    int in_string;
    int in_character;
    int escaped;

    in_string = 0;
    in_character = 0;
    escaped = 0;

    for (position = line; *position != '\0'; ++position) {
        if (*in_block_comment) {
            if (position[0] == '*' &&
                position[1] == '/') {
                position[0] = ' ';
                position[1] = ' ';
                ++position;
                *in_block_comment = 0;
            } else {
                *position = ' ';
            }
            continue;
        }

        if (in_string) {
            if (escaped) {
                escaped = 0;
            } else if (*position == '\\') {
                escaped = 1;
            } else if (*position == '"') {
                in_string = 0;
            }
            *position = ' ';
            continue;
        }

        if (in_character) {
            if (escaped) {
                escaped = 0;
            } else if (*position == '\\') {
                escaped = 1;
            } else if (*position == '\'') {
                in_character = 0;
            }
            *position = ' ';
            continue;
        }

        if (position[0] == '/' &&
            position[1] == '*') {
            position[0] = ' ';
            position[1] = ' ';
            ++position;
            *in_block_comment = 1;
            continue;
        }

        if (position[0] == '/' &&
            position[1] == '/') {
            while (*position != '\0') {
                *position++ = ' ';
            }
            break;
        }

        if (*position == '"') {
            in_string = 1;
            *position = ' ';
        } else if (*position == '\'') {
            in_character = 1;
            *position = ' ';
        }
    }
}

static int symbol_extract_candidate(
    const char *line,
    char *symbol,
    size_t symbol_size,
    const char **name_position_out)
{
    const char *position;
    const char *after;
    const char *last_candidate;
    char last_name[SYMBOL_NAME_SIZE];

    position = line;
    last_candidate = NULL;
    last_name[0] = '\0';

    while (*position != '\0') {
        const char *start;
        size_t length;

        while (*position != '\0' &&
               !(isalpha((unsigned char)*position) ||
                 *position == '_')) {
            ++position;
        }

        if (*position == '\0') {
            break;
        }

        start = position++;
        while (symbol_is_identifier_char(
                   (unsigned char)*position)) {
            ++position;
        }

        length = (size_t)(position - start);
        if (length == 0U || length >= sizeof(last_name)) {
            continue;
        }

        (void)memcpy(last_name, start, length);
        last_name[length] = '\0';

        if (symbol_is_keyword(last_name)) {
            continue;
        }

        after = position;
        while (*after != '\0' &&
               isspace((unsigned char)*after)) {
            ++after;
        }

        if (*after == '(') {
            last_candidate = start;
        }
    }

    if (last_candidate == NULL ||
        last_name[0] == '\0' ||
        strlen(last_name) >= symbol_size) {
        return 0;
    }

    (void)strcpy(symbol, last_name);
    if (name_position_out != NULL) {
        *name_position_out = last_candidate;
    }

    return 1;
}

static int symbol_candidate_is_definition(
    const char *line,
    const char *name_position,
    const char *symbol)
{
    const char *before;
    const char *after;
    const char *close_paren;
    const char *brace;
    const char *semicolon;

    before = name_position;
    while (before > line &&
           isspace((unsigned char)before[-1])) {
        --before;
    }

    /*
     * A top-level function definition must have declaration text before the
     * name. This rejects continuation-line calls and expression calls.
     */
    if (before == line) {
        return 0;
    }

    after = name_position + strlen(symbol);
    while (*after != '\0' &&
           isspace((unsigned char)*after)) {
        ++after;
    }

    if (*after != '(') {
        return 0;
    }

    close_paren = strrchr(after, ')');
    if (close_paren == NULL) {
        return 0;
    }

    brace = strchr(close_paren, '{');
    semicolon = strchr(close_paren, ';');

    return brace != NULL &&
           (semicolon == NULL || brace < semicolon);
}

static int symbol_candidate_is_member_call(
    const char *line,
    const char *name_position)
{
    const char *before;

    before = name_position;
    while (before > line &&
           isspace((unsigned char)before[-1])) {
        --before;
    }

    if (before >= line + 2 &&
        before[-2] == '-' &&
        before[-1] == '>') {
        return 1;
    }

    return before > line &&
           before[-1] == '.';
}

typedef struct symbol_scan_state {
    unsigned int brace_depth;
    unsigned int paren_depth;
    int in_block_comment;
    int in_string;
    int in_character;
    int escaped;
    int pending_definition;
    int pending_signature;
    char pending_name[SYMBOL_NAME_SIZE];
    unsigned long pending_line;
} symbol_scan_state;

static void symbol_scan_state_init(symbol_scan_state *state)
{
    (void)memset(state, 0, sizeof(*state));
}

static int symbol_is_control_word(const char *name)
{
    return strcmp(name, "if") == 0 ||
           strcmp(name, "for") == 0 ||
           strcmp(name, "while") == 0 ||
           strcmp(name, "switch") == 0 ||
           strcmp(name, "return") == 0 ||
           strcmp(name, "sizeof") == 0;
}

static int symbol_is_member_access(const char *line,
                                   const char *position)
{
    const char *before;

    before = position;

    while (before > line &&
           isspace((unsigned char)before[-1])) {
        --before;
    }

    if (before >= line + 2 &&
        before[-2] == '-' &&
        before[-1] == '>') {
        return 1;
    }

    return before > line && before[-1] == '.';
}

static int symbol_has_declaration_prefix(const char *line,
                                         const char *name_position)
{
    const char *position;

    position = line;

    while (position < name_position &&
           isspace((unsigned char)*position)) {
        ++position;
    }

    if (position >= name_position) {
        return 0;
    }

    if (strncmp(position, "if", 2U) == 0 ||
        strncmp(position, "for", 3U) == 0 ||
        strncmp(position, "while", 5U) == 0 ||
        strncmp(position, "switch", 6U) == 0 ||
        strncmp(position, "return", 6U) == 0) {
        return 0;
    }

    return 1;
}

static int symbol_find_matching_paren(const char *start,
                                      const char **after_out)
{
    const char *position;
    int depth;
    int in_string;
    int in_character;
    int escaped;

    if (start == NULL || *start != '(') {
        return 0;
    }

    depth = 0;
    in_string = 0;
    in_character = 0;
    escaped = 0;

    for (position = start; *position != '\0'; ++position) {
        if (in_string) {
            if (escaped) {
                escaped = 0;
            } else if (*position == '\\') {
                escaped = 1;
            } else if (*position == '"') {
                in_string = 0;
            }
            continue;
        }

        if (in_character) {
            if (escaped) {
                escaped = 0;
            } else if (*position == '\\') {
                escaped = 1;
            } else if (*position == '\'') {
                in_character = 0;
            }
            continue;
        }

        if (*position == '"') {
            in_string = 1;
        } else if (*position == '\'') {
            in_character = 1;
        } else if (*position == '(') {
            ++depth;
        } else if (*position == ')') {
            --depth;

            if (depth == 0) {
                if (after_out != NULL) {
                    *after_out = position + 1;
                }
                return 1;
            }
        }
    }

    return 0;
}

static unsigned int symbol_emit_record(FILE *index_file,
                                       char kind,
                                       const char *name,
                                       const char *path,
                                       unsigned long line_number)
{
    if (fprintf(
            index_file,
            "%c|%s|%s|%lu\n",
            kind,
            name,
            path,
            line_number) < 0) {
        return 0U;
    }

    return 1U;
}

static unsigned int symbol_scan_line(FILE *index_file,
                                     const char *path,
                                     unsigned long line_number,
                                     char *line,
                                     symbol_scan_state *state)
{
    const char *position;
    unsigned int entries;

    entries = 0U;
    position = line;

    while (*position != '\0') {
        if (state->in_block_comment) {
            if (position[0] == '*' &&
                position[1] == '/') {
                state->in_block_comment = 0;
                position += 2;
            } else {
                ++position;
            }
            continue;
        }

        if (state->in_string) {
            if (state->escaped) {
                state->escaped = 0;
            } else if (*position == '\\') {
                state->escaped = 1;
            } else if (*position == '"') {
                state->in_string = 0;
            }
            ++position;
            continue;
        }

        if (state->in_character) {
            if (state->escaped) {
                state->escaped = 0;
            } else if (*position == '\\') {
                state->escaped = 1;
            } else if (*position == '\'') {
                state->in_character = 0;
            }
            ++position;
            continue;
        }

        if (position[0] == '/' &&
            position[1] == '*') {
            state->in_block_comment = 1;
            position += 2;
            continue;
        }

        if (position[0] == '/' &&
            position[1] == '/') {
            break;
        }

        if (*position == '"') {
            state->in_string = 1;
            ++position;
            continue;
        }

        if (*position == '\'') {
            state->in_character = 1;
            ++position;
            continue;
        }

        if (*position == '(') {
            if (state->pending_signature) {
                ++state->paren_depth;
            }
            ++position;
            continue;
        }

        if (*position == ')') {
            if (state->pending_signature &&
                state->paren_depth > 0U) {
                --state->paren_depth;

                if (state->paren_depth == 0U) {
                    state->pending_signature = 0;
                    state->pending_definition = 1;
                }
            }
            ++position;
            continue;
        }

        if (*position == '{') {
            if (state->brace_depth == 0U &&
                state->pending_definition) {
                entries += symbol_emit_record(
                    index_file,
                    'D',
                    state->pending_name,
                    path,
                    state->pending_line
                );
                state->pending_definition = 0;
                state->pending_signature = 0;
                state->pending_name[0] = '\0';
            }

            ++state->brace_depth;
            ++position;
            continue;
        }

        if (*position == '}') {
            if (state->brace_depth > 0U) {
                --state->brace_depth;
            }
            ++position;
            continue;
        }

        if (*position == ';') {
            if (state->brace_depth == 0U) {
                state->pending_definition = 0;
                state->pending_signature = 0;
                state->paren_depth = 0U;
                state->pending_name[0] = '\0';
            }
            ++position;
            continue;
        }

        if (isalpha((unsigned char)*position) ||
            *position == '_') {
            const char *name_start;
            const char *after_name;
            char name[SYMBOL_NAME_SIZE];
            size_t length;

            name_start = position++;
            while (symbol_is_identifier_char(
                       (unsigned char)*position)) {
                ++position;
            }

            length = (size_t)(position - name_start);
            if (length == 0U || length >= sizeof(name)) {
                continue;
            }

            (void)memcpy(name, name_start, length);
            name[length] = '\0';

            if (symbol_is_keyword(name) ||
                symbol_is_control_word(name)) {
                continue;
            }

            after_name = position;
            while (*after_name != '\0' &&
                   isspace((unsigned char)*after_name)) {
                ++after_name;
            }

            if (*after_name != '(') {
                continue;
            }

            if (state->brace_depth == 0U) {
                if (symbol_has_declaration_prefix(
                        line,
                        name_start) &&
                    !symbol_is_member_access(
                        line,
                        name_start)) {
                    const char *after_paren;

                    after_paren = NULL;

                    if (symbol_find_matching_paren(
                            after_name,
                            &after_paren)) {
                        while (*after_paren != '\0' &&
                               isspace((unsigned char)*after_paren)) {
                            ++after_paren;
                        }

                        if (*after_paren == '{') {
                            entries += symbol_emit_record(
                                index_file,
                                'D',
                                name,
                                path,
                                line_number
                            );
                        } else if (*after_paren == '\0') {
                            state->pending_definition = 1;
                            (void)strcpy(
                                state->pending_name,
                                name
                            );
                            state->pending_line = line_number;
                        }
                    } else {
                        /*
                         * Parameter list continues onto later lines.
                         * Track parentheses until the signature closes.
                         */
                        state->pending_signature = 1;
                        state->paren_depth = 1U;
                        (void)strcpy(
                            state->pending_name,
                            name
                        );
                        state->pending_line = line_number;

                        for (after_paren = after_name + 1;
                             *after_paren != '\0';
                             ++after_paren) {
                            if (*after_paren == '(') {
                                ++state->paren_depth;
                            } else if (*after_paren == ')' &&
                                       state->paren_depth > 0U) {
                                --state->paren_depth;
                            }
                        }

                        if (state->paren_depth == 0U) {
                            state->pending_signature = 0;
                            state->pending_definition = 1;
                        }
                    }
                }
            } else if (!symbol_is_member_access(
                           line,
                           name_start)) {
                entries += symbol_emit_record(
                    index_file,
                    'C',
                    name,
                    path,
                    line_number
                );
            }

            position = after_name + 1;
            continue;
        }

        ++position;
    }

    return entries;
}

static int symbol_write_manifest_record(FILE *file,
                                        const char *path)
{
    struct stat st;

    if (stat(path, &st) != 0) {
        return 0;
    }

    return fprintf(
        file,
        "F|%s|%lu|%lu\n",
        path,
        (unsigned long)st.st_size,
        (unsigned long)st.st_mtime
    ) >= 0;
}

static unsigned int symbol_build_index(void)
{
    DIR *directory;
    struct dirent *entry;
    FILE *index_file;
    unsigned int files;
    unsigned int entries;
    time_t now;

    directory = opendir("SRC");

    if (directory == NULL) {
        (void)puts("Unable to open SRC directory.");
        return 0U;
    }

    index_file = fopen(SYMBOL_INDEX_FILE, "w");

    if (index_file == NULL) {
        (void)closedir(directory);
        (void)printf(
            "Unable to create %s.\n",
            SYMBOL_INDEX_FILE
        );
        return 0U;
    }

    now = time(NULL);

    (void)fprintf(
        index_file,
        "# OVMS Agent symbol index version %d\n",
        SYMBOL_INDEX_VERSION
    );
    (void)fprintf(
        index_file,
        "# generated=%lu\n",
        (unsigned long)now
    );

    files = 0U;
    entries = 0U;

    while ((entry = readdir(directory)) != NULL) {
        FILE *source_file;
        char path[SYMBOL_PATH_SIZE];
        char line[SYMBOL_LINE_SIZE];
        unsigned long line_number;
        symbol_scan_state scan_state;

        if (!symbol_has_c_extension(entry->d_name)) {
            continue;
        }

        if ((size_t)snprintf(
                path,
                sizeof(path),
                "SRC/%s",
                entry->d_name) >= sizeof(path)) {
            continue;
        }

        source_file = fopen(path, "r");

        if (source_file == NULL) {
            continue;
        }

        if (!symbol_write_manifest_record(index_file, path)) {
            (void)fclose(source_file);
            continue;
        }

        ++files;
        line_number = 0UL;
        symbol_scan_state_init(&scan_state);

        while (fgets(line, sizeof(line), source_file) != NULL) {
            ++line_number;
            entries += symbol_scan_line(
                index_file,
                path,
                line_number,
                line,
                &scan_state
            );
        }

        (void)fclose(source_file);
    }

    (void)fclose(index_file);
    (void)closedir(directory);

    (void)printf(
        "Symbol index created: %u source file%s, "
        "%u entr%s.\n",
        files,
        files == 1U ? "" : "s",
        entries,
        entries == 1U ? "y" : "ies"
    );

    return entries;
}

static int symbol_parse_manifest(char *line,
                                 symbol_manifest_record *record)
{
    char *first;
    char *second;
    char *newline;
    char *end;

    if (line == NULL ||
        record == NULL ||
        strncmp(line, SYMBOL_MANIFEST_PREFIX, 2U) != 0) {
        return 0;
    }

    first = strchr(line + 2, '|');
    if (first == NULL) return 0;
    second = strchr(first + 1, '|');
    if (second == NULL) return 0;
    newline = strchr(second + 1, '\n');
    if (newline != NULL) *newline = '\0';
    *first = '\0';
    *second = '\0';

    if (strlen(line + 2) >= sizeof(record->path)) return 0;
    (void)strcpy(record->path, line + 2);

    record->size = strtoul(first + 1, &end, 10);
    if (end == first + 1 || *end != '\0') return 0;
    record->mtime = strtoul(second + 1, &end, 10);
    return end != second + 1 && *end == '\0';
}

static int symbol_manifest_current(unsigned int *checked_out,
                                   unsigned int *changed_out,
                                   int verbose)
{
    FILE *file;
    char line[SYMBOL_LINE_SIZE];
    unsigned int checked;
    unsigned int changed;

    file = fopen(SYMBOL_INDEX_FILE, "r");
    if (file == NULL) {
        if (checked_out != NULL) *checked_out = 0U;
        if (changed_out != NULL) *changed_out = 0U;
        return 0;
    }

    checked = 0U;
    changed = 0U;

    while (fgets(line, sizeof(line), file) != NULL) {
        symbol_manifest_record record;
        struct stat st;

        if (!symbol_parse_manifest(line, &record)) continue;
        ++checked;

        if (stat(record.path, &st) != 0 ||
            (unsigned long)st.st_size != record.size ||
            (unsigned long)st.st_mtime != record.mtime) {
            ++changed;
            if (verbose) (void)printf("  %s\n", record.path);
        }
    }

    (void)fclose(file);
    if (checked_out != NULL) *checked_out = checked;
    if (changed_out != NULL) *changed_out = changed;
    return checked > 0U && changed == 0U;
}

static int symbol_parse_record(char *line,
                               char *kind,
                               char **symbol,
                               char **path,
                               unsigned long *line_number)
{
    char *first;
    char *second;
    char *third;
    char *end;

    if (line == NULL ||
        line[0] == '#' ||
        line[0] == 'F' ||
        line[0] == '\0') {
        return 0;
    }

    first = strchr(line, '|');

    if (first == NULL) {
        return 0;
    }

    second = strchr(first + 1, '|');

    if (second == NULL) {
        return 0;
    }

    third = strchr(second + 1, '|');

    if (third == NULL) {
        return 0;
    }

    *first = '\0';
    *second = '\0';
    *third = '\0';

    if (line[0] == '\0' ||
        line[1] != '\0') {
        return 0;
    }

    *kind = line[0];
    *symbol = first + 1;
    *path = second + 1;
    *line_number = strtoul(third + 1, &end, 10);

    return *symbol[0] != '\0' &&
           *path[0] != '\0' &&
           end != third + 1;
}

static unsigned int symbol_query_index(
    const char *symbol,
    int show_definitions,
    int show_calls)
{
    FILE *file;
    char line[SYMBOL_LINE_SIZE];
    unsigned int matches;

    file = fopen(SYMBOL_INDEX_FILE, "r");

    if (file == NULL) {
        return 0U;
    }

    matches = 0U;

    while (fgets(line, sizeof(line), file) != NULL) {
        char kind;
        char *record_symbol;
        char *path;
        unsigned long line_number;

        if (!symbol_parse_record(
                line,
                &kind,
                &record_symbol,
                &path,
                &line_number)) {
            continue;
        }

        if (strcmp(record_symbol, symbol) != 0) {
            continue;
        }

        if ((kind == 'D' && show_definitions) ||
            (kind == 'C' && show_calls)) {
            (void)printf(
                "%-4s %-32s line %lu\n",
                kind == 'D' ? "DEF" : "CALL",
                path,
                line_number
            );
            ++matches;
        }
    }

    (void)fclose(file);
    return matches;
}

static unsigned int symbol_scan_live(const char *symbol,
                                     int show_definitions,
                                     int show_calls)
{
    DIR *directory;
    struct dirent *entry;
    unsigned int matches;

    directory = opendir("SRC");

    if (directory == NULL) {
        (void)puts("Unable to open SRC directory.");
        return 0U;
    }

    matches = 0U;

    while ((entry = readdir(directory)) != NULL) {
        FILE *file;
        char path[SYMBOL_PATH_SIZE];
        char line[SYMBOL_LINE_SIZE];
        unsigned long line_number;
        int in_block_comment;

        if (!symbol_has_c_extension(entry->d_name)) {
            continue;
        }

        if ((size_t)snprintf(
                path,
                sizeof(path),
                "SRC/%s",
                entry->d_name) >= sizeof(path)) {
            continue;
        }

        file = fopen(path, "r");

        if (file == NULL) {
            continue;
        }

        line_number = 0UL;
        in_block_comment = 0;

        while (fgets(line, sizeof(line), file) != NULL) {
            int matched;

            ++line_number;
            symbol_clean_line(line, &in_block_comment);
            matched = 0;

            if (show_definitions &&
                symbol_line_is_definition(line, symbol)) {
                (void)printf(
                    "DEF  %-32s line %lu\n",
                    path,
                    line_number
                );
                matched = 1;
            }

            if (show_calls &&
                symbol_line_is_call(line, symbol)) {
                (void)printf(
                    "CALL %-32s line %lu\n",
                    path,
                    line_number
                );
                matched = 1;
            }

            if (matched) {
                ++matches;
            }
        }

        (void)fclose(file);
    }

    (void)closedir(directory);
    return matches;
}

static unsigned int symbol_query(const char *symbol,
                                 int show_definitions,
                                 int show_calls)
{
    FILE *file;
    unsigned int checked;
    unsigned int changed;

    file = fopen(SYMBOL_INDEX_FILE, "r");
    if (file == NULL) {
        (void)printf(
            "%s not found; using live source scan.\n",
            SYMBOL_INDEX_FILE
        );
        return symbol_scan_live(symbol, show_definitions, show_calls);
    }
    (void)fclose(file);

    if (!symbol_manifest_current(&checked, &changed, 0)) {
        (void)puts("Symbol index is stale.");
        (void)puts("Using live source scan.");
        (void)puts("Run REINDEX to refresh.");
        return symbol_scan_live(symbol, show_definitions, show_calls);
    }

    return symbol_query_index(symbol, show_definitions, show_calls);
}

static int symbol_prepare(agent_state *state,
                          const char *symbol)
{
    if (state == NULL ||
        state->project_root == NULL ||
        *state->project_root == '\0') {
        (void)puts("OVMS_AGENT_ROOT is not defined.");
        return 0;
    }

    if (!symbol_name_valid(symbol)) {
        (void)puts(
            "Symbol must be a C identifier containing only letters, "
            "digits, and underscores."
        );
        return 0;
    }

    return 1;
}

void symbol_reindex(agent_state *state)
{
    if (state == NULL ||
        state->project_root == NULL ||
        *state->project_root == '\0') {
        (void)puts("OVMS_AGENT_ROOT is not defined.");
        return;
    }

    (void)puts("Building persistent C symbol index...");
    (void)symbol_build_index();
}

void symbol_index_check(agent_state *state)
{
    FILE *file;
    unsigned int checked;
    unsigned int changed;

    if (state == NULL ||
        state->project_root == NULL ||
        *state->project_root == '\0') {
        (void)puts("OVMS_AGENT_ROOT is not defined.");
        return;
    }

    file = fopen(SYMBOL_INDEX_FILE, "r");
    if (file == NULL) {
        (void)printf("Symbol index is missing (%s).\n", SYMBOL_INDEX_FILE);
        (void)puts("Run REINDEX to create it.");
        return;
    }
    (void)fclose(file);

    (void)puts("Checking symbol database...");
    if (symbol_manifest_current(&checked, &changed, 1)) {
        (void)printf("%u file%s checked.\n", checked, checked == 1U ? "" : "s");
        (void)puts("Current.");
    } else {
        (void)printf("%u file%s checked; %u changed.\n",
                     checked, checked == 1U ? "" : "s", changed);
        (void)puts("Run REINDEX.");
    }
}

void symbol_index_status(agent_state *state)
{
    FILE *file;
    char line[SYMBOL_LINE_SIZE];
    unsigned int definitions;
    unsigned int calls;
    unsigned int manifest_files;
    unsigned int checked;
    unsigned int changed;
    int current;

    if (state == NULL ||
        state->project_root == NULL ||
        *state->project_root == '\0') {
        (void)puts("OVMS_AGENT_ROOT is not defined.");
        return;
    }

    file = fopen(SYMBOL_INDEX_FILE, "r");
    if (file == NULL) {
        (void)printf("Symbol index: missing (%s)\n", SYMBOL_INDEX_FILE);
        (void)puts("Run REINDEX to create it.");
        return;
    }

    definitions = 0U;
    calls = 0U;
    manifest_files = 0U;
    while (fgets(line, sizeof(line), file) != NULL) {
        if (line[0] == 'D' && line[1] == '|') ++definitions;
        else if (line[0] == 'C' && line[1] == '|') ++calls;
        else if (line[0] == 'F' && line[1] == '|') ++manifest_files;
    }
    (void)fclose(file);

    current = symbol_manifest_current(&checked, &changed, 0);
    (void)printf("Symbol index: %s\n", SYMBOL_INDEX_FILE);
    (void)printf("Status:       %s\n", current ? "CURRENT" : "STALE");
    (void)printf("Source files: %u\n", manifest_files);
    (void)printf("Definitions:  %u\n", definitions);
    (void)printf("Call sites:   %u\n", calls);
    (void)printf("Total entries: %u\n", definitions + calls);
    if (!current) {
        (void)printf("Changed files: %u\n", changed);
        (void)puts("Run INDEX/CHECK for details.");
    }
}

void symbol_where(agent_state *state, const char *symbol)
{
    unsigned int matches;

    if (!symbol_prepare(state, symbol)) {
        return;
    }

    (void)printf("Definitions of %s\n", symbol);
    (void)puts("----------------------------------------");
    matches = symbol_query(symbol, 1, 0);

    if (matches == 0U) {
        (void)puts("No definitions found.");
    }
}

void symbol_callers(agent_state *state, const char *symbol)
{
    unsigned int matches;

    if (!symbol_prepare(state, symbol)) {
        return;
    }

    (void)printf("Callers of %s\n", symbol);
    (void)puts("----------------------------------------");
    matches = symbol_query(symbol, 0, 1);

    if (matches == 0U) {
        (void)puts("No call sites found.");
    }
}

void symbol_show(agent_state *state, const char *symbol)
{
    unsigned int definitions;
    unsigned int calls;

    if (!symbol_prepare(state, symbol)) {
        return;
    }

    (void)printf("Symbol: %s\n", symbol);
    (void)puts("Definitions");
    (void)puts("----------------------------------------");
    definitions = symbol_query(symbol, 1, 0);

    if (definitions == 0U) {
        (void)puts("No definitions found.");
    }

    (void)puts("");
    (void)puts("Call sites");
    (void)puts("----------------------------------------");
    calls = symbol_query(symbol, 0, 1);

    if (calls == 0U) {
        (void)puts("No call sites found.");
    }

    (void)puts("");
    (void)printf(
        "Summary: %u definition match%s, %u call-site match%s.\n",
        definitions,
        definitions == 1U ? "" : "es",
        calls,
        calls == 1U ? "" : "es"
    );
}

static int symbol_source_line_is_definition(
    const char *path,
    unsigned long target_line,
    const char *symbol)
{
    FILE *file;
    char line[SYMBOL_LINE_SIZE];
    unsigned long line_number;
    symbol_scan_state state;

    file = fopen(path, "r");

    if (file == NULL) {
        return 0;
    }

    line_number = 0UL;
    symbol_scan_state_init(&state);

    while (fgets(line, sizeof(line), file) != NULL) {
        FILE *sink;
        unsigned int before;

        ++line_number;
        before = state.pending_definition;

        sink = tmpfile();

        if (sink == NULL) {
            (void)fclose(file);
            return 0;
        }

        (void)symbol_scan_line(
            sink,
            path,
            line_number,
            line,
            &state
        );

        (void)fclose(sink);

        if (line_number == target_line &&
            state.pending_definition &&
            strcmp(state.pending_name, symbol) == 0) {
            (void)fclose(file);
            return 1;
        }

        if (line_number == target_line &&
            !before &&
            strstr(line, symbol) != NULL &&
            state.brace_depth > 0U) {
            /*
             * Same-line opening brace definitions are already emitted by the
             * scanner. Validate them with the conservative prefix rule.
             */
            const char *position;

            position = strstr(line, symbol);
            if (position != NULL &&
                symbol_has_declaration_prefix(
                    line,
                    position)) {
                (void)fclose(file);
                return 1;
            }
        }
    }

    (void)fclose(file);
    return 0;
}

static int symbol_find_definition_record(
    const char *symbol,
    char *path_out,
    size_t path_size,
    unsigned long *line_out)
{
    FILE *file;
    char line[SYMBOL_LINE_SIZE];

    file = fopen(SYMBOL_INDEX_FILE, "r");

    if (file == NULL) {
        return 0;
    }

    while (fgets(line, sizeof(line), file) != NULL) {
        char kind;
        char *record_symbol;
        char *path;
        unsigned long line_number;

        if (!symbol_parse_record(
                line,
                &kind,
                &record_symbol,
                &path,
                &line_number)) {
            continue;
        }

        if (kind == 'D' &&
            strcmp(record_symbol, symbol) == 0 &&
            symbol_source_line_is_definition(
                path,
                line_number,
                symbol)) {
            if (strlen(path) >= path_size) {
                (void)fclose(file);
                return 0;
            }

            (void)strcpy(path_out, path);
            *line_out = line_number;
            (void)fclose(file);
            return 1;
        }
    }

    (void)fclose(file);
    return 0;
}

static int symbol_find_definition_live(
    const char *symbol,
    char *path_out,
    size_t path_size,
    unsigned long *line_out)
{
    DIR *directory;
    struct dirent *entry;

    directory = opendir("SRC");

    if (directory == NULL) {
        return 0;
    }

    while ((entry = readdir(directory)) != NULL) {
        FILE *file;
        char path[SYMBOL_PATH_SIZE];
        char line[SYMBOL_LINE_SIZE];
        unsigned long line_number;
        int in_block_comment;

        if (!symbol_has_c_extension(entry->d_name)) {
            continue;
        }

        if ((size_t)snprintf(
                path,
                sizeof(path),
                "SRC/%s",
                entry->d_name) >= sizeof(path)) {
            continue;
        }

        file = fopen(path, "r");

        if (file == NULL) {
            continue;
        }

        line_number = 0UL;
        in_block_comment = 0;

        while (fgets(line, sizeof(line), file) != NULL) {
            char clean[SYMBOL_LINE_SIZE];

            ++line_number;
            (void)strncpy(clean, line, sizeof(clean) - 1U);
            clean[sizeof(clean) - 1U] = '\0';
            symbol_clean_line(clean, &in_block_comment);

            if (symbol_line_is_definition(clean, symbol)) {
                if (strlen(path) < path_size) {
                    (void)strcpy(path_out, path);
                    *line_out = line_number;
                    (void)fclose(file);
                    (void)closedir(directory);
                    return 1;
                }
            }
        }

        (void)fclose(file);
    }

    (void)closedir(directory);
    return 0;
}

static int symbol_locate_definition(
    const char *symbol,
    char *path_out,
    size_t path_size,
    unsigned long *line_out)
{
    unsigned int checked;
    unsigned int changed;

    if (symbol_manifest_current(
            &checked,
            &changed,
            0) &&
        symbol_find_definition_record(
            symbol,
            path_out,
            path_size,
            line_out)) {
        return 1;
    }

    return symbol_find_definition_live(
        symbol,
        path_out,
        path_size,
        line_out
    );
}

static int symbol_print_function_body(
    const char *path,
    unsigned long start_line)
{
    FILE *file;
    char line[SYMBOL_LINE_SIZE];
    unsigned long line_number;
    int depth;
    int started;
    int in_block_comment;
    int in_string;
    int in_character;
    int escaped;

    file = fopen(path, "r");

    if (file == NULL) {
        return 0;
    }

    line_number = 0UL;
    depth = 0;
    started = 0;
    in_block_comment = 0;
    in_string = 0;
    in_character = 0;
    escaped = 0;

    while (fgets(line, sizeof(line), file) != NULL) {
        char *position;

        ++line_number;

        if (line_number < start_line) {
            continue;
        }

        (void)printf("%6lu  %s", line_number, line);

        for (position = line;
             *position != '\0';
             ++position) {
            if (in_block_comment) {
                if (position[0] == '*' &&
                    position[1] == '/') {
                    ++position;
                    in_block_comment = 0;
                }
                continue;
            }

            if (in_string) {
                if (escaped) {
                    escaped = 0;
                } else if (*position == '\\') {
                    escaped = 1;
                } else if (*position == '"') {
                    in_string = 0;
                }
                continue;
            }

            if (in_character) {
                if (escaped) {
                    escaped = 0;
                } else if (*position == '\\') {
                    escaped = 1;
                } else if (*position == '\'') {
                    in_character = 0;
                }
                continue;
            }

            if (position[0] == '/' &&
                position[1] == '*') {
                ++position;
                in_block_comment = 1;
                continue;
            }

            if (position[0] == '/' &&
                position[1] == '/') {
                break;
            }

            if (*position == '"') {
                in_string = 1;
                continue;
            }

            if (*position == '\'') {
                in_character = 1;
                continue;
            }

            if (*position == '{') {
                ++depth;
                started = 1;
            } else if (*position == '}' && started) {
                --depth;

                if (depth == 0) {
                    (void)fclose(file);
                    return 1;
                }
            }
        }
    }

    (void)fclose(file);
    return started;
}

void symbol_show_function(agent_state *state,
                          const char *symbol)
{
    char path[SYMBOL_PATH_SIZE];
    unsigned long line_number;

    if (!symbol_prepare(state, symbol)) {
        return;
    }

    path[0] = '\0';
    line_number = 0UL;

    if (!symbol_locate_definition(
            symbol,
            path,
            sizeof(path),
            &line_number)) {
        (void)printf(
            "No function definition found for %s.\n",
            symbol
        );
        return;
    }

    (void)printf(
        "Function %s defined in %s at line %lu\n",
        symbol,
        path,
        line_number
    );
    (void)puts("----------------------------------------");

    if (!symbol_print_function_body(
            path,
            line_number)) {
        (void)puts(
            "Unable to determine the complete function body."
        );
    }
}

static int symbol_has_header_extension(const char *name)
{
    size_t length;

    if (name == NULL) {
        return 0;
    }

    length = strlen(name);

    return length >= 2U &&
           name[length - 2U] == '.' &&
           (name[length - 1U] == 'H' ||
            name[length - 1U] == 'h');
}

static int symbol_line_starts_type_definition(
    const char *line,
    const char *symbol,
    const char **kind_out)
{
    static const char *kinds[] = {
        "struct",
        "union",
        "enum",
        NULL
    };
    const char **kind;
    const char *position;
    const char *name_start;
    size_t length;

    if (line == NULL || symbol == NULL) {
        return 0;
    }

    position = line;

    while (*position != '\0' &&
           isspace((unsigned char)*position)) {
        ++position;
    }

    if (strncmp(position, "typedef", 7U) == 0 &&
        isspace((unsigned char)position[7])) {
        position += 7;

        while (*position != '\0' &&
               isspace((unsigned char)*position)) {
            ++position;
        }
    }

    for (kind = kinds; *kind != NULL; ++kind) {
        length = strlen(*kind);

        if (strncmp(position, *kind, length) != 0 ||
            !isspace((unsigned char)position[length])) {
            continue;
        }

        position += length;

        while (*position != '\0' &&
               isspace((unsigned char)*position)) {
            ++position;
        }

        name_start = position;

        while (symbol_is_identifier_char(
                   (unsigned char)*position)) {
            ++position;
        }

        if ((size_t)(position - name_start) == strlen(symbol) &&
            strncmp(name_start, symbol, strlen(symbol)) == 0) {
            if (kind_out != NULL) {
                *kind_out = *kind;
            }

            return 1;
        }
    }

    return 0;
}

static int symbol_find_type_definition(
    const char *symbol,
    char *path_out,
    size_t path_size,
    unsigned long *line_out,
    char *kind_out,
    size_t kind_size)
{
    DIR *directory;
    struct dirent *entry;

    directory = opendir("SRC");

    if (directory == NULL) {
        return 0;
    }

    while ((entry = readdir(directory)) != NULL) {
        FILE *file;
        char path[SYMBOL_PATH_SIZE];
        char line[SYMBOL_LINE_SIZE];
        unsigned long line_number;
        int in_block_comment;

        if (!symbol_has_c_extension(entry->d_name) &&
            !symbol_has_header_extension(entry->d_name)) {
            continue;
        }

        if ((size_t)snprintf(
                path,
                sizeof(path),
                "SRC/%s",
                entry->d_name) >= sizeof(path)) {
            continue;
        }

        file = fopen(path, "r");

        if (file == NULL) {
            continue;
        }

        line_number = 0UL;
        in_block_comment = 0;

        while (fgets(line, sizeof(line), file) != NULL) {
            char clean[SYMBOL_LINE_SIZE];
            const char *kind;

            ++line_number;
            (void)strncpy(clean, line, sizeof(clean) - 1U);
            clean[sizeof(clean) - 1U] = '\0';
            symbol_clean_line(clean, &in_block_comment);

            kind = NULL;

            if (symbol_line_starts_type_definition(
                    clean,
                    symbol,
                    &kind)) {
                if (strlen(path) < path_size &&
                    strlen(kind) < kind_size) {
                    (void)strcpy(path_out, path);
                    (void)strcpy(kind_out, kind);
                    *line_out = line_number;
                    (void)fclose(file);
                    (void)closedir(directory);
                    return 1;
                }
            }
        }

        (void)fclose(file);
    }

    (void)closedir(directory);
    return 0;
}

static int symbol_print_braced_definition(
    const char *path,
    unsigned long start_line)
{
    FILE *file;
    char line[SYMBOL_LINE_SIZE];
    unsigned long line_number;
    int depth;
    int started;
    int in_block_comment;
    int in_string;
    int in_character;
    int escaped;

    file = fopen(path, "r");

    if (file == NULL) {
        return 0;
    }

    line_number = 0UL;
    depth = 0;
    started = 0;
    in_block_comment = 0;
    in_string = 0;
    in_character = 0;
    escaped = 0;

    while (fgets(line, sizeof(line), file) != NULL) {
        char *position;

        ++line_number;

        if (line_number < start_line) {
            continue;
        }

        (void)printf("%6lu  %s", line_number, line);

        for (position = line;
             *position != '\0';
             ++position) {
            if (in_block_comment) {
                if (position[0] == '*' &&
                    position[1] == '/') {
                    ++position;
                    in_block_comment = 0;
                }
                continue;
            }

            if (in_string) {
                if (escaped) {
                    escaped = 0;
                } else if (*position == '\\') {
                    escaped = 1;
                } else if (*position == '"') {
                    in_string = 0;
                }
                continue;
            }

            if (in_character) {
                if (escaped) {
                    escaped = 0;
                } else if (*position == '\\') {
                    escaped = 1;
                } else if (*position == '\'') {
                    in_character = 0;
                }
                continue;
            }

            if (position[0] == '/' &&
                position[1] == '*') {
                ++position;
                in_block_comment = 1;
                continue;
            }

            if (position[0] == '/' &&
                position[1] == '/') {
                break;
            }

            if (*position == '"') {
                in_string = 1;
                continue;
            }

            if (*position == '\'') {
                in_character = 1;
                continue;
            }

            if (*position == '{') {
                ++depth;
                started = 1;
            } else if (*position == '}' && started) {
                --depth;

                if (depth == 0) {
                    /*
                     * Continue until the terminating semicolon when it is
                     * present on this or the following line.
                     */
                    if (strchr(position, ';') != NULL) {
                        (void)fclose(file);
                        return 1;
                    }
                }
            } else if (*position == ';' &&
                       started &&
                       depth == 0) {
                (void)fclose(file);
                return 1;
            }
        }
    }

    (void)fclose(file);
    return started;
}

void symbol_show_struct(agent_state *state,
                        const char *symbol)
{
    char path[SYMBOL_PATH_SIZE];
    char kind[16];
    unsigned long line_number;

    if (!symbol_prepare(state, symbol)) {
        return;
    }

    path[0] = '\0';
    kind[0] = '\0';
    line_number = 0UL;

    if (!symbol_find_type_definition(
            symbol,
            path,
            sizeof(path),
            &line_number,
            kind,
            sizeof(kind))) {
        (void)printf(
            "No struct, union, or enum definition found for %s.\n",
            symbol
        );
        return;
    }

    (void)printf(
        "%s %s defined in %s at line %lu\n",
        kind,
        symbol,
        path,
        line_number
    );
    (void)puts("----------------------------------------");

    if (!symbol_print_braced_definition(
            path,
            line_number)) {
        (void)puts(
            "Unable to determine the complete type definition."
        );
    }
}

#define MODULE_ITEM_MAX 256U

typedef struct symbol_name_list {
    char names[MODULE_ITEM_MAX][SYMBOL_NAME_SIZE];
    unsigned int used;
} symbol_name_list;

static int module_name_list_contains(
    const symbol_name_list *list,
    const char *name)
{
    unsigned int index;

    for (index = 0U; index < list->used; ++index) {
        if (strcmp(list->names[index], name) == 0) {
            return 1;
        }
    }

    return 0;
}

static void module_name_list_add(
    symbol_name_list *list,
    const char *name)
{
    if (list == NULL ||
        name == NULL ||
        *name == '\0' ||
        list->used >= MODULE_ITEM_MAX ||
        strlen(name) >= SYMBOL_NAME_SIZE ||
        module_name_list_contains(list, name)) {
        return;
    }

    (void)strcpy(list->names[list->used], name);
    ++list->used;
}

static int module_path_prepare(
    const char *module,
    char *path,
    size_t path_size)
{
    size_t length;

    if (module == NULL ||
        *module == '\0' ||
        strchr(module, ':') != NULL ||
        strstr(module, "..") != NULL ||
        *module == '/') {
        return 0;
    }

    if (strncmp(module, "SRC/", 4U) == 0 ||
        strncmp(module, "src/", 4U) == 0) {
        if (strlen(module) >= path_size) {
            return 0;
        }

        (void)strcpy(path, module);
    } else {
        if ((size_t)snprintf(
                path,
                path_size,
                "SRC/%s",
                module) >= path_size) {
            return 0;
        }
    }

    length = strlen(path);

    return length >= 2U &&
           path[length - 2U] == '.' &&
           (path[length - 1U] == 'C' ||
            path[length - 1U] == 'c' ||
            path[length - 1U] == 'H' ||
            path[length - 1U] == 'h');
}

static void module_extract_include(
    const char *line,
    symbol_name_list *includes)
{
    const char *position;
    const char *end;
    char name[SYMBOL_NAME_SIZE];
    size_t length;

    position = line;

    while (*position != '\0' &&
           isspace((unsigned char)*position)) {
        ++position;
    }

    if (*position != '#') {
        return;
    }

    ++position;

    while (*position != '\0' &&
           isspace((unsigned char)*position)) {
        ++position;
    }

    if (strncmp(position, "include", 7U) != 0) {
        return;
    }

    position += 7;

    while (*position != '\0' &&
           isspace((unsigned char)*position)) {
        ++position;
    }

    if (*position == '"') {
        ++position;
        end = strchr(position, '"');
    } else if (*position == '<') {
        ++position;
        end = strchr(position, '>');
    } else {
        return;
    }

    if (end == NULL) {
        return;
    }

    length = (size_t)(end - position);

    if (length == 0U ||
        length >= sizeof(name)) {
        return;
    }

    (void)memcpy(name, position, length);
    name[length] = '\0';
    module_name_list_add(includes, name);
}

static int module_load_index_symbols(
    const char *path,
    symbol_name_list *definitions,
    symbol_name_list *calls)
{
    FILE *file;
    char line[SYMBOL_LINE_SIZE];
    unsigned int checked;
    unsigned int changed;

    if (!symbol_manifest_current(
            &checked,
            &changed,
            0)) {
        return 0;
    }

    file = fopen(SYMBOL_INDEX_FILE, "r");

    if (file == NULL) {
        return 0;
    }

    while (fgets(line, sizeof(line), file) != NULL) {
        char kind;
        char *record_symbol;
        char *record_path;
        unsigned long line_number;

        if (!symbol_parse_record(
                line,
                &kind,
                &record_symbol,
                &record_path,
                &line_number)) {
            continue;
        }

        if (strcmp(record_path, path) != 0) {
            continue;
        }

        if (kind == 'D') {
            module_name_list_add(
                definitions,
                record_symbol
            );
        } else if (kind == 'C') {
            module_name_list_add(
                calls,
                record_symbol
            );
        }
    }

    (void)fclose(file);
    return 1;
}

static void module_extract_type(
    const char *line,
    symbol_name_list *types)
{
    static const char *kinds[] = {
        "struct",
        "union",
        "enum",
        NULL
    };
    const char *position;
    const char **kind;

    position = line;

    while (*position != '\0' &&
           isspace((unsigned char)*position)) {
        ++position;
    }

    if (strncmp(position, "typedef", 7U) == 0 &&
        isspace((unsigned char)position[7])) {
        position += 7;

        while (*position != '\0' &&
               isspace((unsigned char)*position)) {
            ++position;
        }
    }

    for (kind = kinds; *kind != NULL; ++kind) {
        size_t kind_length;
        const char *name_start;
        char name[SYMBOL_NAME_SIZE];
        size_t name_length;

        kind_length = strlen(*kind);

        if (strncmp(position, *kind, kind_length) != 0 ||
            !isspace((unsigned char)position[kind_length])) {
            continue;
        }

        position += kind_length;

        while (*position != '\0' &&
               isspace((unsigned char)*position)) {
            ++position;
        }

        name_start = position;

        while (symbol_is_identifier_char(
                   (unsigned char)*position)) {
            ++position;
        }

        name_length = (size_t)(position - name_start);

        if (name_length == 0U ||
            name_length >= sizeof(name)) {
            return;
        }

        (void)memcpy(name, name_start, name_length);
        name[name_length] = '\0';
        module_name_list_add(types, name);
        return;
    }
}

static void module_print_list(
    const char *heading,
    const symbol_name_list *list)
{
    unsigned int index;

    (void)printf("%s (%u)\n", heading, list->used);
    (void)puts("----------------------------------------");

    if (list->used == 0U) {
        (void)puts("  none");
        return;
    }

    for (index = 0U; index < list->used; ++index) {
        (void)printf("  %s\n", list->names[index]);
    }
}

void symbol_show_module(agent_state *state,
                        const char *module)
{
    char path[SYMBOL_PATH_SIZE];
    FILE *file;
    char line[SYMBOL_LINE_SIZE];
    char clean[SYMBOL_LINE_SIZE];
    unsigned long line_count;
    int in_block_comment;
    symbol_name_list includes;
    symbol_name_list definitions;
    symbol_name_list calls;
    symbol_name_list types;

    if (state == NULL ||
        state->project_root == NULL ||
        *state->project_root == '\0') {
        (void)puts("OVMS_AGENT_ROOT is not defined.");
        return;
    }

    if (!module_path_prepare(
            module,
            path,
            sizeof(path))) {
        (void)puts(
            "Module must be a project-relative .C or .H file."
        );
        return;
    }

    file = fopen(path, "r");

    if (file == NULL) {
        (void)printf("Unable to open %s.\n", path);
        return;
    }

    (void)memset(&includes, 0, sizeof(includes));
    (void)memset(&definitions, 0, sizeof(definitions));
    (void)memset(&calls, 0, sizeof(calls));
    (void)memset(&types, 0, sizeof(types));

    line_count = 0UL;
    in_block_comment = 0;

    while (fgets(line, sizeof(line), file) != NULL) {
        ++line_count;
        module_extract_include(line, &includes);

        (void)strncpy(clean, line, sizeof(clean) - 1U);
        clean[sizeof(clean) - 1U] = '\0';
        symbol_clean_line(clean, &in_block_comment);

        module_extract_type(clean, &types);
    }

    (void)fclose(file);

    if (!module_load_index_symbols(
            path,
            &definitions,
            &calls)) {
        (void)puts(
            "Symbol index is missing or stale; "
            "run REINDEX for function lists."
        );
    }

    /*
     * A definition should not also appear in the call list for the same
     * module summary.
     */
    {
        unsigned int source_index;
        symbol_name_list filtered_calls;

        (void)memset(
            &filtered_calls,
            0,
            sizeof(filtered_calls)
        );

        for (source_index = 0U;
             source_index < calls.used;
             ++source_index) {
            if (!module_name_list_contains(
                    &definitions,
                    calls.names[source_index])) {
                module_name_list_add(
                    &filtered_calls,
                    calls.names[source_index]
                );
            }
        }

        calls = filtered_calls;
    }

    (void)printf("Module: %s\n", path);
    (void)printf("Lines:  %lu\n", line_count);
    (void)puts("");

    module_print_list("Includes", &includes);
    (void)puts("");
    module_print_list("Functions defined", &definitions);
    (void)puts("");
    module_print_list("Functions called", &calls);
    (void)puts("");
    module_print_list("Types declared", &types);
}

static int dependency_header_is_project_local(
    const char *header,
    char *path_out,
    size_t path_size)
{
    FILE *file;
    char path[SYMBOL_PATH_SIZE];

    if (header == NULL ||
        *header == '\0' ||
        strchr(header, '/') != NULL ||
        strchr(header, '\\') != NULL) {
        return 0;
    }

    if ((size_t)snprintf(
            path,
            sizeof(path),
            "SRC/%s",
            header) >= sizeof(path)) {
        return 0;
    }

    file = fopen(path, "r");

    if (file == NULL) {
        return 0;
    }

    (void)fclose(file);

    if (strlen(path) >= path_size) {
        return 0;
    }

    (void)strcpy(path_out, path);
    return 1;
}

static int dependency_find_definition_module(
    const char *symbol,
    char *path_out,
    size_t path_size)
{
    FILE *file;
    char line[SYMBOL_LINE_SIZE];

    file = fopen(SYMBOL_INDEX_FILE, "r");

    if (file == NULL) {
        return 0;
    }

    while (fgets(line, sizeof(line), file) != NULL) {
        char kind;
        char *record_symbol;
        char *record_path;
        unsigned long line_number;

        if (!symbol_parse_record(
                line,
                &kind,
                &record_symbol,
                &record_path,
                &line_number)) {
            continue;
        }

        if (kind != 'D' ||
            strcmp(record_symbol, symbol) != 0) {
            continue;
        }

        if (strlen(record_path) >= path_size) {
            (void)fclose(file);
            return 0;
        }

        (void)strcpy(path_out, record_path);
        (void)fclose(file);
        return 1;
    }

    (void)fclose(file);
    return 0;
}

static void dependency_collect_includes(
    const char *path,
    symbol_name_list *project_headers,
    symbol_name_list *system_headers)
{
    FILE *file;
    char line[SYMBOL_LINE_SIZE];

    file = fopen(path, "r");

    if (file == NULL) {
        return;
    }

    while (fgets(line, sizeof(line), file) != NULL) {
        const char *position;
        const char *end;
        char header[SYMBOL_NAME_SIZE];
        char local_path[SYMBOL_PATH_SIZE];
        size_t length;

        position = line;

        while (*position != '\0' &&
               isspace((unsigned char)*position)) {
            ++position;
        }

        if (*position != '#') {
            continue;
        }

        ++position;

        while (*position != '\0' &&
               isspace((unsigned char)*position)) {
            ++position;
        }

        if (strncmp(position, "include", 7U) != 0) {
            continue;
        }

        position += 7;

        while (*position != '\0' &&
               isspace((unsigned char)*position)) {
            ++position;
        }

        if (*position == '"') {
            ++position;
            end = strchr(position, '"');
        } else if (*position == '<') {
            ++position;
            end = strchr(position, '>');
        } else {
            continue;
        }

        if (end == NULL) {
            continue;
        }

        length = (size_t)(end - position);

        if (length == 0U ||
            length >= sizeof(header)) {
            continue;
        }

        (void)memcpy(header, position, length);
        header[length] = '\0';

        if (dependency_header_is_project_local(
                header,
                local_path,
                sizeof(local_path))) {
            module_name_list_add(
                project_headers,
                local_path
            );
        } else {
            module_name_list_add(
                system_headers,
                header
            );
        }
    }

    (void)fclose(file);
}

static int dependency_load_module_calls(
    const char *path,
    symbol_name_list *calls)
{
    FILE *file;
    char line[SYMBOL_LINE_SIZE];
    unsigned int checked;
    unsigned int changed;

    if (!symbol_manifest_current(
            &checked,
            &changed,
            0)) {
        return 0;
    }

    file = fopen(SYMBOL_INDEX_FILE, "r");

    if (file == NULL) {
        return 0;
    }

    while (fgets(line, sizeof(line), file) != NULL) {
        char kind;
        char *record_symbol;
        char *record_path;
        unsigned long line_number;

        if (!symbol_parse_record(
                line,
                &kind,
                &record_symbol,
                &record_path,
                &line_number)) {
            continue;
        }

        if (kind == 'C' &&
            strcmp(record_path, path) == 0) {
            module_name_list_add(
                calls,
                record_symbol
            );
        }
    }

    (void)fclose(file);
    return 1;
}

static void dependency_print_mapping(
    const char *heading,
    const symbol_name_list *symbols,
    int show_defining_module)
{
    unsigned int index;

    (void)printf("%s (%u)\n", heading, symbols->used);
    (void)puts("----------------------------------------");

    if (symbols->used == 0U) {
        (void)puts("  none");
        return;
    }

    for (index = 0U;
         index < symbols->used;
         ++index) {
        if (show_defining_module) {
            char path[SYMBOL_PATH_SIZE];

            path[0] = '\0';

            if (dependency_find_definition_module(
                    symbols->names[index],
                    path,
                    sizeof(path))) {
                (void)printf(
                    "  %-32s %s\n",
                    symbols->names[index],
                    path
                );
            } else {
                (void)printf(
                    "  %-32s external or unresolved\n",
                    symbols->names[index]
                );
            }
        } else {
            (void)printf(
                "  %s\n",
                symbols->names[index]
            );
        }
    }
}

void symbol_show_dependencies(agent_state *state,
                              const char *module)
{
    char path[SYMBOL_PATH_SIZE];
    symbol_name_list project_headers;
    symbol_name_list system_headers;
    symbol_name_list calls;
    symbol_name_list project_modules;
    symbol_name_list external_calls;
    unsigned int index;

    if (state == NULL ||
        state->project_root == NULL ||
        *state->project_root == '\0') {
        (void)puts("OVMS_AGENT_ROOT is not defined.");
        return;
    }

    if (!module_path_prepare(
            module,
            path,
            sizeof(path))) {
        (void)puts(
            "Module must be a project-relative .C or .H file."
        );
        return;
    }

    {
        FILE *file;

        file = fopen(path, "r");

        if (file == NULL) {
            (void)printf("Unable to open %s.\n", path);
            return;
        }

        (void)fclose(file);
    }

    (void)memset(
        &project_headers,
        0,
        sizeof(project_headers)
    );
    (void)memset(
        &system_headers,
        0,
        sizeof(system_headers)
    );
    (void)memset(&calls, 0, sizeof(calls));
    (void)memset(
        &project_modules,
        0,
        sizeof(project_modules)
    );
    (void)memset(
        &external_calls,
        0,
        sizeof(external_calls)
    );

    dependency_collect_includes(
        path,
        &project_headers,
        &system_headers
    );

    if (!dependency_load_module_calls(path, &calls)) {
        (void)puts(
            "Symbol index is missing or stale; "
            "run REINDEX before DEPENDENCIES."
        );
        return;
    }

    for (index = 0U;
         index < calls.used;
         ++index) {
        char definition_path[SYMBOL_PATH_SIZE];

        definition_path[0] = '\0';

        if (dependency_find_definition_module(
                calls.names[index],
                definition_path,
                sizeof(definition_path))) {
            if (strcmp(definition_path, path) != 0) {
                module_name_list_add(
                    &project_modules,
                    definition_path
                );
            }
        } else {
            module_name_list_add(
                &external_calls,
                calls.names[index]
            );
        }
    }

    (void)printf("Dependencies for %s\n", path);
    (void)puts("");

    module_print_list(
        "Project headers",
        &project_headers
    );
    (void)puts("");

    module_print_list(
        "System or external headers",
        &system_headers
    );
    (void)puts("");

    module_print_list(
        "Called project modules",
        &project_modules
    );
    (void)puts("");

    dependency_print_mapping(
        "External or unresolved calls",
        &external_calls,
        0
    );
    (void)puts("");

    dependency_print_mapping(
        "All indexed calls",
        &calls,
        1
    );
}

static const char *usage_kind_name(
    int is_definition,
    int is_call,
    int is_declaration,
    int is_type_reference)
{
    if (is_definition) {
        return "DEF";
    }

    if (is_call) {
        return "CALL";
    }

    if (is_type_reference) {
        return "TYPE";
    }

    if (is_declaration) {
        return "DECL";
    }

    return "REF";
}

static int usage_line_has_declaration_prefix(
    const char *line,
    const char *position)
{
    const char *cursor;

    cursor = line;

    while (cursor < position &&
           isspace((unsigned char)*cursor)) {
        ++cursor;
    }

    if (cursor >= position) {
        return 0;
    }

    if (strncmp(cursor, "extern", 6U) == 0 ||
        strncmp(cursor, "static", 6U) == 0 ||
        strncmp(cursor, "const", 5U) == 0 ||
        strncmp(cursor, "volatile", 8U) == 0 ||
        strncmp(cursor, "unsigned", 8U) == 0 ||
        strncmp(cursor, "signed", 6U) == 0 ||
        strncmp(cursor, "short", 5U) == 0 ||
        strncmp(cursor, "long", 4U) == 0 ||
        strncmp(cursor, "int", 3U) == 0 ||
        strncmp(cursor, "char", 4U) == 0 ||
        strncmp(cursor, "void", 4U) == 0 ||
        strncmp(cursor, "float", 5U) == 0 ||
        strncmp(cursor, "double", 6U) == 0 ||
        strncmp(cursor, "struct", 6U) == 0 ||
        strncmp(cursor, "union", 5U) == 0 ||
        strncmp(cursor, "enum", 4U) == 0 ||
        strncmp(cursor, "typedef", 7U) == 0) {
        return 1;
    }

    return 0;
}

static int usage_line_is_type_reference(
    const char *line,
    const char *position)
{
    const char *end;
    const char *start;
    size_t length;

    if (line == NULL ||
        position == NULL ||
        position <= line) {
        return 0;
    }

    end = position;

    while (end > line &&
           isspace((unsigned char)end[-1])) {
        --end;
    }

    start = end;

    while (start > line &&
           symbol_is_identifier_char(
               (unsigned char)start[-1])) {
        --start;
    }

    length = (size_t)(end - start);

    return (length == 6U &&
            strncmp(start, "struct", 6U) == 0) ||
           (length == 5U &&
            strncmp(start, "union", 5U) == 0) ||
           (length == 4U &&
            strncmp(start, "enum", 4U) == 0);
}

static int usage_line_is_call(
    const char *line,
    const char *position,
    const char *symbol)
{
    const char *after;

    if (symbol_is_member_access(line, position)) {
        return 0;
    }

    after = position + strlen(symbol);

    while (*after != '\0' &&
           isspace((unsigned char)*after)) {
        ++after;
    }

    return *after == '(';
}

static int usage_line_is_definition(
    const char *path,
    unsigned long line_number,
    const char *symbol)
{
    FILE *file;
    char line[SYMBOL_LINE_SIZE];

    file = fopen(SYMBOL_INDEX_FILE, "r");

    if (file == NULL) {
        return 0;
    }

    while (fgets(line, sizeof(line), file) != NULL) {
        char kind;
        char *record_symbol;
        char *record_path;
        unsigned long record_line;

        if (!symbol_parse_record(
                line,
                &kind,
                &record_symbol,
                &record_path,
                &record_line)) {
            continue;
        }

        if (kind == 'D' &&
            record_line == line_number &&
            strcmp(record_symbol, symbol) == 0 &&
            strcmp(record_path, path) == 0) {
            (void)fclose(file);
            return 1;
        }
    }

    (void)fclose(file);
    return 0;
}

static int usage_line_is_indexed_call(
    const char *path,
    unsigned long line_number,
    const char *symbol)
{
    FILE *file;
    char line[SYMBOL_LINE_SIZE];

    file = fopen(SYMBOL_INDEX_FILE, "r");

    if (file == NULL) {
        return 0;
    }

    while (fgets(line, sizeof(line), file) != NULL) {
        char kind;
        char *record_symbol;
        char *record_path;
        unsigned long record_line;

        if (!symbol_parse_record(
                line,
                &kind,
                &record_symbol,
                &record_path,
                &record_line)) {
            continue;
        }

        if (kind == 'C' &&
            record_line == line_number &&
            strcmp(record_symbol, symbol) == 0 &&
            strcmp(record_path, path) == 0) {
            (void)fclose(file);
            return 1;
        }
    }

    (void)fclose(file);
    return 0;
}

void symbol_who_uses(agent_state *state,
                     const char *symbol)
{
    DIR *directory;
    struct dirent *entry;
    unsigned int matches;
    unsigned int checked;
    unsigned int changed;

    if (!symbol_prepare(state, symbol)) {
        return;
    }

    if (!symbol_manifest_current(
            &checked,
            &changed,
            0)) {
        (void)puts(
            "Symbol index is missing or stale; "
            "run REINDEX before WHO/USES."
        );
        return;
    }

    directory = opendir("SRC");

    if (directory == NULL) {
        (void)puts("Unable to open SRC directory.");
        return;
    }

    (void)printf("Uses of %s\n", symbol);
    (void)puts("----------------------------------------");

    matches = 0U;

    while ((entry = readdir(directory)) != NULL) {
        FILE *file;
        char path[SYMBOL_PATH_SIZE];
        char line[SYMBOL_LINE_SIZE];
        char clean[SYMBOL_LINE_SIZE];
        unsigned long line_number;
        int in_block_comment;

        if (!symbol_has_c_extension(entry->d_name) &&
            !symbol_has_header_extension(entry->d_name)) {
            continue;
        }

        if ((size_t)snprintf(
                path,
                sizeof(path),
                "SRC/%s",
                entry->d_name) >= sizeof(path)) {
            continue;
        }

        file = fopen(path, "r");

        if (file == NULL) {
            continue;
        }

        line_number = 0UL;
        in_block_comment = 0;

        while (fgets(line, sizeof(line), file) != NULL) {
            const char *position;

            ++line_number;
            (void)strncpy(clean, line, sizeof(clean) - 1U);
            clean[sizeof(clean) - 1U] = '\0';
            symbol_clean_line(clean, &in_block_comment);

            position = clean;

            while ((position = strstr(position, symbol)) != NULL) {
                int is_definition;
                int is_call;
                int is_declaration;
                int is_type_reference;

                if (!symbol_match_at(clean, position, symbol)) {
                    ++position;
                    continue;
                }

                is_definition = usage_line_is_definition(
                    path,
                    line_number,
                    symbol
                );
                is_call = usage_line_is_indexed_call(
                    path,
                    line_number,
                    symbol
                );
                is_type_reference = usage_line_is_type_reference(
                    clean,
                    position
                );
                is_declaration =
                    !is_definition &&
                    !is_call &&
                    usage_line_has_declaration_prefix(
                        clean,
                        position
                    );

                /*
                 * Only use syntax as a call fallback when the occurrence
                 * is not already known to be a declaration or type use.
                 * This prevents header prototypes from appearing as calls.
                 */
                if (!is_call &&
                    !is_declaration &&
                    !is_type_reference) {
                    is_call = usage_line_is_call(
                        clean,
                        position,
                        symbol
                    );
                }

                (void)printf(
                    "%-4s %-32s line %lu\n",
                    usage_kind_name(
                        is_definition,
                        is_call,
                        is_declaration,
                        is_type_reference
                    ),
                    path,
                    line_number
                );

                ++matches;
                position += strlen(symbol);
            }
        }

        (void)fclose(file);
    }

    (void)closedir(directory);

    if (matches == 0U) {
        (void)puts("No uses found.");
    } else {
        (void)printf(
            "Total matches: %u\n",
            matches
        );
    }
}

#define MODULE_EDGE_MAX 1024U

typedef struct module_edge {
    char source[SYMBOL_PATH_SIZE];
    char target[SYMBOL_PATH_SIZE];
} module_edge;

typedef struct module_edge_list {
    module_edge edges[MODULE_EDGE_MAX];
    unsigned int used;
} module_edge_list;

static int module_edge_exists(
    const module_edge_list *list,
    const char *source,
    const char *target)
{
    unsigned int index;

    for (index = 0U; index < list->used; ++index) {
        if (strcmp(list->edges[index].source, source) == 0 &&
            strcmp(list->edges[index].target, target) == 0) {
            return 1;
        }
    }

    return 0;
}

static void module_edge_add(
    module_edge_list *list,
    const char *source,
    const char *target)
{
    if (list == NULL ||
        source == NULL ||
        target == NULL ||
        *source == '\0' ||
        *target == '\0' ||
        strcmp(source, target) == 0 ||
        list->used >= MODULE_EDGE_MAX ||
        strlen(source) >= SYMBOL_PATH_SIZE ||
        strlen(target) >= SYMBOL_PATH_SIZE ||
        module_edge_exists(list, source, target)) {
        return;
    }

    (void)strcpy(list->edges[list->used].source, source);
    (void)strcpy(list->edges[list->used].target, target);
    ++list->used;
}

static int module_graph_current(void)
{
    unsigned int checked;
    unsigned int changed;

    return symbol_manifest_current(
        &checked,
        &changed,
        0
    );
}

static int module_graph_collect_one(
    const char *source_path,
    module_edge_list *edges)
{
    FILE *file;
    char line[SYMBOL_LINE_SIZE];

    file = fopen(SYMBOL_INDEX_FILE, "r");

    if (file == NULL) {
        return 0;
    }

    while (fgets(line, sizeof(line), file) != NULL) {
        char kind;
        char *record_symbol;
        char *record_path;
        unsigned long line_number;
        char target_path[SYMBOL_PATH_SIZE];

        if (!symbol_parse_record(
                line,
                &kind,
                &record_symbol,
                &record_path,
                &line_number)) {
            continue;
        }

        if (kind != 'C' ||
            strcmp(record_path, source_path) != 0) {
            continue;
        }

        target_path[0] = '\0';

        if (dependency_find_definition_module(
                record_symbol,
                target_path,
                sizeof(target_path))) {
            module_edge_add(
                edges,
                source_path,
                target_path
            );
        }
    }

    (void)fclose(file);
    return 1;
}

static int module_graph_collect_all(
    module_edge_list *edges)
{
    FILE *file;
    char line[SYMBOL_LINE_SIZE];

    file = fopen(SYMBOL_INDEX_FILE, "r");

    if (file == NULL) {
        return 0;
    }

    while (fgets(line, sizeof(line), file) != NULL) {
        char kind;
        char *record_symbol;
        char *record_path;
        unsigned long line_number;
        char target_path[SYMBOL_PATH_SIZE];

        if (!symbol_parse_record(
                line,
                &kind,
                &record_symbol,
                &record_path,
                &line_number)) {
            continue;
        }

        if (kind != 'C') {
            continue;
        }

        target_path[0] = '\0';

        if (dependency_find_definition_module(
                record_symbol,
                target_path,
                sizeof(target_path))) {
            module_edge_add(
                edges,
                record_path,
                target_path
            );
        }
    }

    (void)fclose(file);
    return 1;
}

static void module_graph_print(
    const char *title,
    const module_edge_list *edges)
{
    unsigned int index;

    (void)puts(title);
    (void)puts("----------------------------------------");

    if (edges->used == 0U) {
        (void)puts("No project-module edges found.");
        return;
    }

    for (index = 0U; index < edges->used; ++index) {
        (void)printf(
            "%s -> %s\n",
            edges->edges[index].source,
            edges->edges[index].target
        );
    }

    (void)printf(
        "Total edges: %u\n",
        edges->used
    );
}

void symbol_module_graph(agent_state *state,
                         const char *module)
{
    char path[SYMBOL_PATH_SIZE];
    module_edge_list edges;
    FILE *file;

    if (state == NULL ||
        state->project_root == NULL ||
        *state->project_root == '\0') {
        (void)puts("OVMS_AGENT_ROOT is not defined.");
        return;
    }

    if (!module_path_prepare(
            module,
            path,
            sizeof(path))) {
        (void)puts(
            "Module must be a project-relative .C or .H file."
        );
        return;
    }

    file = fopen(path, "r");

    if (file == NULL) {
        (void)printf("Unable to open %s.\n", path);
        return;
    }

    (void)fclose(file);

    if (!module_graph_current()) {
        (void)puts(
            "Symbol index is missing or stale; "
            "run REINDEX before MODULE/GRAPH."
        );
        return;
    }

    (void)memset(&edges, 0, sizeof(edges));

    if (!module_graph_collect_one(path, &edges)) {
        (void)puts("Unable to read the symbol index.");
        return;
    }

    {
        char title[SYMBOL_PATH_SIZE + 64U];

        (void)snprintf(
            title,
            sizeof(title),
            "Module graph for %s",
            path
        );
        module_graph_print(title, &edges);
    }
}

void symbol_module_graph_all(agent_state *state)
{
    module_edge_list edges;

    if (state == NULL ||
        state->project_root == NULL ||
        *state->project_root == '\0') {
        (void)puts("OVMS_AGENT_ROOT is not defined.");
        return;
    }

    if (!module_graph_current()) {
        (void)puts(
            "Symbol index is missing or stale; "
            "run REINDEX before MODULE/GRAPH/ALL."
        );
        return;
    }

    (void)memset(&edges, 0, sizeof(edges));

    if (!module_graph_collect_all(&edges)) {
        (void)puts("Unable to read the symbol index.");
        return;
    }

    module_graph_print(
        "Project module dependency graph",
        &edges
    );
}

static int impact_find_calling_function(
    const char *path,
    unsigned long call_line,
    char *function_out,
    size_t function_size)
{
    FILE *file;
    char line[SYMBOL_LINE_SIZE];
    char best_name[SYMBOL_NAME_SIZE];
    unsigned long best_line;

    file = fopen(SYMBOL_INDEX_FILE, "r");

    if (file == NULL) {
        return 0;
    }

    best_name[0] = '\0';
    best_line = 0UL;

    while (fgets(line, sizeof(line), file) != NULL) {
        char kind;
        char *record_symbol;
        char *record_path;
        unsigned long record_line;

        if (!symbol_parse_record(
                line,
                &kind,
                &record_symbol,
                &record_path,
                &record_line)) {
            continue;
        }

        if (kind != 'D' ||
            strcmp(record_path, path) != 0 ||
            record_line > call_line ||
            record_line < best_line) {
            continue;
        }

        if (strlen(record_symbol) >= sizeof(best_name)) {
            continue;
        }

        (void)strcpy(best_name, record_symbol);
        best_line = record_line;
    }

    (void)fclose(file);

    if (best_name[0] == '\0' ||
        strlen(best_name) >= function_size) {
        return 0;
    }

    (void)strcpy(function_out, best_name);
    return 1;
}

static int impact_find_definition(
    const char *symbol,
    char *path_out,
    size_t path_size,
    unsigned long *line_out)
{
    FILE *file;
    char line[SYMBOL_LINE_SIZE];

    file = fopen(SYMBOL_INDEX_FILE, "r");

    if (file == NULL) {
        return 0;
    }

    while (fgets(line, sizeof(line), file) != NULL) {
        char kind;
        char *record_symbol;
        char *record_path;
        unsigned long record_line;

        if (!symbol_parse_record(
                line,
                &kind,
                &record_symbol,
                &record_path,
                &record_line)) {
            continue;
        }

        if (kind == 'D' &&
            strcmp(record_symbol, symbol) == 0) {
            if (strlen(record_path) >= path_size) {
                (void)fclose(file);
                return 0;
            }

            (void)strcpy(path_out, record_path);
            *line_out = record_line;
            (void)fclose(file);
            return 1;
        }
    }

    (void)fclose(file);
    return 0;
}

static int impact_collect_callers(
    const char *symbol,
    symbol_name_list *functions,
    symbol_name_list *modules)
{
    FILE *file;
    char line[SYMBOL_LINE_SIZE];

    file = fopen(SYMBOL_INDEX_FILE, "r");

    if (file == NULL) {
        return 0;
    }

    while (fgets(line, sizeof(line), file) != NULL) {
        char kind;
        char *record_symbol;
        char *record_path;
        unsigned long record_line;

        if (!symbol_parse_record(
                line,
                &kind,
                &record_symbol,
                &record_path,
                &record_line)) {
            continue;
        }

        if (kind != 'C' ||
            strcmp(record_symbol, symbol) != 0) {
            continue;
        }

        module_name_list_add(
            modules,
            record_path
        );

        {
            char function_name[SYMBOL_NAME_SIZE];

            function_name[0] = '\0';

            if (impact_find_calling_function(
                    record_path,
                    record_line,
                    function_name,
                    sizeof(function_name))) {
                module_name_list_add(
                    functions,
                    function_name
                );
            }
        }
    }

    (void)fclose(file);
    return 1;
}

static void impact_print_functions(
    const symbol_name_list *functions)
{
    unsigned int index;

    (void)printf(
        "Directly affected functions (%u)\n",
        functions->used
    );
    (void)puts("----------------------------------------");

    if (functions->used == 0U) {
        (void)puts("  none");
        return;
    }

    for (index = 0U;
         index < functions->used;
         ++index) {
        (void)printf(
            "  %s\n",
            functions->names[index]
        );
    }
}

static void impact_print_modules(
    const symbol_name_list *modules)
{
    unsigned int index;

    (void)printf(
        "Directly affected modules (%u)\n",
        modules->used
    );
    (void)puts("----------------------------------------");

    if (modules->used == 0U) {
        (void)puts("  none");
        return;
    }

    for (index = 0U;
         index < modules->used;
         ++index) {
        (void)printf(
            "  %s\n",
            modules->names[index]
        );
    }
}

void symbol_impact(agent_state *state,
                   const char *symbol)
{
    unsigned int checked;
    unsigned int changed;
    char definition_path[SYMBOL_PATH_SIZE];
    unsigned long definition_line;
    symbol_name_list functions;
    symbol_name_list modules;

    if (!symbol_prepare(state, symbol)) {
        return;
    }

    if (!symbol_manifest_current(
            &checked,
            &changed,
            0)) {
        (void)puts(
            "Symbol index is missing or stale; "
            "run REINDEX before IMPACT."
        );
        return;
    }

    definition_path[0] = '\0';
    definition_line = 0UL;
    (void)memset(&functions, 0, sizeof(functions));
    (void)memset(&modules, 0, sizeof(modules));

    (void)printf("Impact analysis for %s\n", symbol);
    (void)puts("========================================");

    if (impact_find_definition(
            symbol,
            definition_path,
            sizeof(definition_path),
            &definition_line)) {
        (void)printf(
            "Definition: %s line %lu\n",
            definition_path,
            definition_line
        );
    } else {
        (void)puts(
            "Definition: external, unresolved, or not a function"
        );
    }

    if (!impact_collect_callers(
            symbol,
            &functions,
            &modules)) {
        (void)puts("Unable to read the symbol index.");
        return;
    }

    (void)puts("");
    impact_print_functions(&functions);
    (void)puts("");
    impact_print_modules(&modules);
    (void)puts("");

    (void)printf(
        "Estimated rebuild scope: %u module%s.\n",
        modules.used,
        modules.used == 1U ? "" : "s"
    );

    if (modules.used == 0U) {
        (void)puts(
            "No indexed direct callers were found."
        );
    } else if (modules.used <= 3U) {
        (void)puts("Impact level: LOW");
    } else if (modules.used <= 10U) {
        (void)puts("Impact level: MODERATE");
    } else {
        (void)puts("Impact level: HIGH");
    }
}

void symbol_module_reverse(agent_state *state,
                           const char *module)
{
    char target_path[SYMBOL_PATH_SIZE];
    module_edge_list edges;
    symbol_name_list sources;
    unsigned int index;
    FILE *file;

    if (state == NULL ||
        state->project_root == NULL ||
        *state->project_root == '\0') {
        (void)puts("OVMS_AGENT_ROOT is not defined.");
        return;
    }

    if (!module_path_prepare(
            module,
            target_path,
            sizeof(target_path))) {
        (void)puts(
            "Module must be a project-relative .C or .H file."
        );
        return;
    }

    file = fopen(target_path, "r");

    if (file == NULL) {
        (void)printf(
            "Unable to open %s.\n",
            target_path
        );
        return;
    }

    (void)fclose(file);

    if (!module_graph_current()) {
        (void)puts(
            "Symbol index is missing or stale; "
            "run REINDEX before MODULE/REVERSE."
        );
        return;
    }

    (void)memset(&edges, 0, sizeof(edges));
    (void)memset(&sources, 0, sizeof(sources));

    if (!module_graph_collect_all(&edges)) {
        (void)puts("Unable to read the symbol index.");
        return;
    }

    for (index = 0U;
         index < edges.used;
         ++index) {
        if (strcmp(
                edges.edges[index].target,
                target_path) == 0) {
            module_name_list_add(
                &sources,
                edges.edges[index].source
            );
        }
    }

    (void)printf(
        "Modules depending on %s (%u)\n",
        target_path,
        sources.used
    );
    (void)puts("----------------------------------------");

    if (sources.used == 0U) {
        (void)puts("  none");
        return;
    }

    for (index = 0U;
         index < sources.used;
         ++index) {
        (void)printf(
            "  %s\n",
            sources.names[index]
        );
    }
}

#define PATH_NODE_MAX 256U

typedef struct path_node {
    char path[SYMBOL_PATH_SIZE];
    int predecessor;
    int visited;
} path_node;

static int path_token_is_module(const char *token)
{
    size_t length;

    if (token == NULL) {
        return 0;
    }

    length = strlen(token);

    return length >= 2U &&
           token[length - 2U] == '.' &&
           (token[length - 1U] == 'C' ||
            token[length - 1U] == 'c' ||
            token[length - 1U] == 'H' ||
            token[length - 1U] == 'h');
}

static int path_resolve_token(
    const char *token,
    char *path_out,
    size_t path_size)
{
    unsigned long line_number;

    if (path_token_is_module(token)) {
        return module_path_prepare(
            token,
            path_out,
            path_size
        );
    }

    line_number = 0UL;

    return impact_find_definition(
        token,
        path_out,
        path_size,
        &line_number
    );
}

static int path_node_find(
    path_node *nodes,
    unsigned int node_count,
    const char *path)
{
    unsigned int index;

    for (index = 0U; index < node_count; ++index) {
        if (strcmp(nodes[index].path, path) == 0) {
            return (int)index;
        }
    }

    return -1;
}

static int path_node_add(
    path_node *nodes,
    unsigned int *node_count,
    const char *path)
{
    int existing;

    existing = path_node_find(
        nodes,
        *node_count,
        path
    );

    if (existing >= 0) {
        return existing;
    }

    if (*node_count >= PATH_NODE_MAX ||
        strlen(path) >= SYMBOL_PATH_SIZE) {
        return -1;
    }

    (void)strcpy(
        nodes[*node_count].path,
        path
    );
    nodes[*node_count].predecessor = -1;
    nodes[*node_count].visited = 0;

    ++(*node_count);
    return (int)(*node_count - 1U);
}

static int path_build_nodes(
    const module_edge_list *edges,
    path_node *nodes,
    unsigned int *node_count)
{
    unsigned int index;

    *node_count = 0U;

    for (index = 0U;
         index < edges->used;
         ++index) {
        if (path_node_add(
                nodes,
                node_count,
                edges->edges[index].source) < 0 ||
            path_node_add(
                nodes,
                node_count,
                edges->edges[index].target) < 0) {
            return 0;
        }
    }

    return 1;
}

static int path_find_shortest(
    const module_edge_list *edges,
    path_node *nodes,
    unsigned int node_count,
    int source_index,
    int target_index)
{
    int queue[PATH_NODE_MAX];
    unsigned int head;
    unsigned int tail;

    head = 0U;
    tail = 0U;

    nodes[source_index].visited = 1;
    queue[tail++] = source_index;

    while (head < tail) {
        int current;
        unsigned int edge_index;

        current = queue[head++];

        if (current == target_index) {
            return 1;
        }

        for (edge_index = 0U;
             edge_index < edges->used;
             ++edge_index) {
            int next;

            if (strcmp(
                    edges->edges[edge_index].source,
                    nodes[current].path) != 0) {
                continue;
            }

            next = path_node_find(
                nodes,
                node_count,
                edges->edges[edge_index].target
            );

            if (next < 0 ||
                nodes[next].visited) {
                continue;
            }

            nodes[next].visited = 1;
            nodes[next].predecessor = current;
            queue[tail++] = next;
        }
    }

    return 0;
}

static void path_print_result(
    path_node *nodes,
    int source_index,
    int target_index)
{
    int chain[PATH_NODE_MAX];
    unsigned int used;
    int current;

    used = 0U;
    current = target_index;

    while (current >= 0 &&
           used < PATH_NODE_MAX) {
        chain[used++] = current;

        if (current == source_index) {
            break;
        }

        current = nodes[current].predecessor;
    }

    if (used == 0U ||
        chain[used - 1U] != source_index) {
        (void)puts("No dependency path found.");
        return;
    }

    (void)printf(
        "Dependency path (%u edge%s)\n",
        used - 1U,
        used - 1U == 1U ? "" : "s"
    );
    (void)puts("----------------------------------------");

    while (used > 0U) {
        --used;
        (void)printf(
            "%s%s",
            nodes[chain[used]].path,
            used == 0U ? "\n" : " -> "
        );
    }
}

void symbol_path_find(agent_state *state,
                      const char *source,
                      const char *target)
{
    char source_path[SYMBOL_PATH_SIZE];
    char target_path[SYMBOL_PATH_SIZE];
    module_edge_list edges;
    path_node nodes[PATH_NODE_MAX];
    unsigned int node_count;
    int source_index;
    int target_index;
    FILE *file;

    if (state == NULL ||
        state->project_root == NULL ||
        *state->project_root == '\0') {
        (void)puts("OVMS_AGENT_ROOT is not defined.");
        return;
    }

    if (!module_graph_current()) {
        (void)puts(
            "Symbol index is missing or stale; "
            "run REINDEX before PATH."
        );
        return;
    }

    source_path[0] = '\0';
    target_path[0] = '\0';

    if (!path_resolve_token(
            source,
            source_path,
            sizeof(source_path))) {
        (void)printf(
            "Unable to resolve source: %s\n",
            source
        );
        return;
    }

    if (!path_resolve_token(
            target,
            target_path,
            sizeof(target_path))) {
        (void)printf(
            "Unable to resolve target: %s\n",
            target
        );
        return;
    }

    file = fopen(source_path, "r");
    if (file == NULL) {
        (void)printf(
            "Unable to open %s.\n",
            source_path
        );
        return;
    }
    (void)fclose(file);

    file = fopen(target_path, "r");
    if (file == NULL) {
        (void)printf(
            "Unable to open %s.\n",
            target_path
        );
        return;
    }
    (void)fclose(file);

    if (strcmp(source_path, target_path) == 0) {
        (void)printf(
            "Source and target resolve to the same module: %s\n",
            source_path
        );
        return;
    }

    (void)memset(&edges, 0, sizeof(edges));
    (void)memset(nodes, 0, sizeof(nodes));

    if (!module_graph_collect_all(&edges) ||
        !path_build_nodes(
            &edges,
            nodes,
            &node_count)) {
        (void)puts("Unable to construct the project graph.");
        return;
    }

    source_index = path_node_add(
        nodes,
        &node_count,
        source_path
    );
    target_index = path_node_add(
        nodes,
        &node_count,
        target_path
    );

    if (source_index < 0 ||
        target_index < 0) {
        (void)puts("Project graph is too large.");
        return;
    }

    (void)printf(
        "Finding path: %s -> %s\n",
        source_path,
        target_path
    );

    if (!path_find_shortest(
            &edges,
            nodes,
            node_count,
            source_index,
            target_index)) {
        (void)puts("No dependency path found.");
        return;
    }

    path_print_result(
        nodes,
        source_index,
        target_index
    );
}

