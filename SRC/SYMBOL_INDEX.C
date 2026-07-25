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

#define CLUSTER_NODE_MAX PATH_NODE_MAX
#define CLUSTER_MAX PATH_NODE_MAX

typedef struct cluster_state {
    path_node nodes[CLUSTER_NODE_MAX];
    unsigned int node_count;
    int index[CLUSTER_NODE_MAX];
    int lowlink[CLUSTER_NODE_MAX];
    int on_stack[CLUSTER_NODE_MAX];
    int stack[CLUSTER_NODE_MAX];
    unsigned int stack_used;
    int next_index;
    int component[CLUSTER_NODE_MAX];
    unsigned int component_count;
} cluster_state;

typedef struct cluster_summary {
    int component_id;
    unsigned int size;
} cluster_summary;

static void cluster_initialize(
    cluster_state *state,
    const module_edge_list *edges)
{
    unsigned int index;

    (void)memset(state, 0, sizeof(*state));

    (void)path_build_nodes(
        edges,
        state->nodes,
        &state->node_count
    );

    for (index = 0U;
         index < state->node_count;
         ++index) {
        state->index[index] = -1;
        state->lowlink[index] = -1;
        state->component[index] = -1;
    }

    state->next_index = 0;
}

static void cluster_strong_connect(
    cluster_state *state,
    const module_edge_list *edges,
    int node_index)
{
    unsigned int edge_index;

    state->index[node_index] = state->next_index;
    state->lowlink[node_index] = state->next_index;
    ++state->next_index;

    state->stack[state->stack_used++] = node_index;
    state->on_stack[node_index] = 1;

    for (edge_index = 0U;
         edge_index < edges->used;
         ++edge_index) {
        int next_index;

        if (strcmp(
                edges->edges[edge_index].source,
                state->nodes[node_index].path) != 0) {
            continue;
        }

        next_index = path_node_find(
            state->nodes,
            state->node_count,
            edges->edges[edge_index].target
        );

        if (next_index < 0) {
            continue;
        }

        if (state->index[next_index] < 0) {
            cluster_strong_connect(
                state,
                edges,
                next_index
            );

            if (state->lowlink[next_index] <
                state->lowlink[node_index]) {
                state->lowlink[node_index] =
                    state->lowlink[next_index];
            }
        } else if (state->on_stack[next_index] &&
                   state->index[next_index] <
                   state->lowlink[node_index]) {
            state->lowlink[node_index] =
                state->index[next_index];
        }
    }

    if (state->lowlink[node_index] ==
        state->index[node_index]) {
        int member;

        do {
            if (state->stack_used == 0U) {
                break;
            }

            member =
                state->stack[--state->stack_used];
            state->on_stack[member] = 0;
            state->component[member] =
                (int)state->component_count;
        } while (member != node_index);

        ++state->component_count;
    }
}

static void cluster_run(
    cluster_state *state,
    const module_edge_list *edges)
{
    unsigned int index;

    for (index = 0U;
         index < state->node_count;
         ++index) {
        if (state->index[index] < 0) {
            cluster_strong_connect(
                state,
                edges,
                (int)index
            );
        }
    }
}

static void cluster_build_summaries(
    const cluster_state *state,
    cluster_summary *summaries)
{
    unsigned int index;

    for (index = 0U;
         index < state->component_count;
         ++index) {
        summaries[index].component_id = (int)index;
        summaries[index].size = 0U;
    }

    for (index = 0U;
         index < state->node_count;
         ++index) {
        int component_id;

        component_id = state->component[index];

        if (component_id >= 0) {
            ++summaries[component_id].size;
        }
    }
}

static void cluster_sort_summaries(
    cluster_summary *summaries,
    unsigned int count)
{
    unsigned int left;

    for (left = 0U; left < count; ++left) {
        unsigned int right;

        for (right = left + 1U;
             right < count;
             ++right) {
            if (summaries[right].size >
                summaries[left].size) {
                cluster_summary temporary;

                temporary = summaries[left];
                summaries[left] = summaries[right];
                summaries[right] = temporary;
            }
        }
    }
}

static void cluster_print_component(
    const cluster_state *state,
    int component_id,
    unsigned int display_number,
    unsigned int size)
{
    unsigned int index;

    (void)printf(
        "Cluster %u (%u module%s)\n",
        display_number,
        size,
        size == 1U ? "" : "s"
    );
    (void)puts("----------------------------------------");

    for (index = 0U;
         index < state->node_count;
         ++index) {
        if (state->component[index] ==
            component_id) {
            (void)printf(
                "  %s\n",
                state->nodes[index].path
            );
        }
    }

    (void)puts("");
}

void symbol_cluster(agent_state *state)
{
    module_edge_list edges;
    cluster_state cluster;
    cluster_summary summaries[CLUSTER_MAX];
    unsigned int index;
    unsigned int multi_count;
    unsigned int singleton_count;

    if (state == NULL ||
        state->project_root == NULL ||
        *state->project_root == '\0') {
        (void)puts("OVMS_AGENT_ROOT is not defined.");
        return;
    }

    if (!module_graph_current()) {
        (void)puts(
            "Symbol index is missing or stale; "
            "run REINDEX before CLUSTER."
        );
        return;
    }

    (void)memset(&edges, 0, sizeof(edges));
    (void)memset(
        summaries,
        0,
        sizeof(summaries)
    );

    if (!module_graph_collect_all(&edges)) {
        (void)puts("Unable to read the symbol index.");
        return;
    }

    cluster_initialize(&cluster, &edges);
    cluster_run(&cluster, &edges);
    cluster_build_summaries(
        &cluster,
        summaries
    );
    cluster_sort_summaries(
        summaries,
        cluster.component_count
    );

    multi_count = 0U;
    singleton_count = 0U;

    for (index = 0U;
         index < cluster.component_count;
         ++index) {
        if (summaries[index].size > 1U) {
            ++multi_count;
        } else {
            ++singleton_count;
        }
    }

    (void)puts("Architectural clusters");
    (void)puts("========================================");
    (void)printf(
        "Modules analyzed: %u\n",
        cluster.node_count
    );
    (void)printf(
        "Multi-module clusters: %u\n",
        multi_count
    );
    (void)printf(
        "Singleton modules: %u\n",
        singleton_count
    );
    (void)puts("");

    {
        unsigned int display_number;

        display_number = 1U;

        for (index = 0U;
             index < cluster.component_count;
             ++index) {
            if (summaries[index].size > 1U) {
                cluster_print_component(
                    &cluster,
                    summaries[index].component_id,
                    display_number++,
                    summaries[index].size
                );
            }
        }
    }

    if (singleton_count > 0U) {
        (void)printf(
            "Singleton modules (%u)\n",
            singleton_count
        );
        (void)puts("----------------------------------------");

        for (index = 0U;
             index < cluster.component_count;
             ++index) {
            unsigned int node_index;

            if (summaries[index].size != 1U) {
                continue;
            }

            for (node_index = 0U;
                 node_index < cluster.node_count;
                 ++node_index) {
                if (cluster.component[node_index] ==
                    summaries[index].component_id) {
                    (void)printf(
                        "  %s\n",
                        cluster.nodes[node_index].path
                    );
                    break;
                }
            }
        }
    }
}

typedef struct cycle_search_state {
    int visited[PATH_NODE_MAX];
    int active[PATH_NODE_MAX];
    int stack[PATH_NODE_MAX];
    unsigned int stack_used;
    int found;
    int cycle_start;
    int cycle_end;
} cycle_search_state;

static void cycle_search_initialize(
    cycle_search_state *state)
{
    (void)memset(state, 0, sizeof(*state));
    state->cycle_start = -1;
    state->cycle_end = -1;
}

static int cycle_find_from(
    const module_edge_list *edges,
    path_node *nodes,
    unsigned int node_count,
    int node_index,
    cycle_search_state *state)
{
    unsigned int edge_index;

    state->visited[node_index] = 1;
    state->active[node_index] = 1;
    state->stack[state->stack_used++] = node_index;

    for (edge_index = 0U;
         edge_index < edges->used;
         ++edge_index) {
        int next_index;

        if (strcmp(
                edges->edges[edge_index].source,
                nodes[node_index].path) != 0) {
            continue;
        }

        next_index = path_node_find(
            nodes,
            node_count,
            edges->edges[edge_index].target
        );

        if (next_index < 0) {
            continue;
        }

        if (!state->visited[next_index]) {
            if (cycle_find_from(
                    edges,
                    nodes,
                    node_count,
                    next_index,
                    state)) {
                return 1;
            }
        } else if (state->active[next_index]) {
            state->found = 1;
            state->cycle_start = next_index;
            state->cycle_end = node_index;
            return 1;
        }
    }

    if (state->stack_used > 0U) {
        --state->stack_used;
    }

    state->active[node_index] = 0;
    return 0;
}

static int cycle_find_any(
    const module_edge_list *edges,
    path_node *nodes,
    unsigned int node_count,
    int preferred_start,
    cycle_search_state *state)
{
    unsigned int index;

    cycle_search_initialize(state);

    if (preferred_start >= 0) {
        return cycle_find_from(
            edges,
            nodes,
            node_count,
            preferred_start,
            state
        );
    }

    for (index = 0U;
         index < node_count;
         ++index) {
        if (!state->visited[index] &&
            cycle_find_from(
                edges,
                nodes,
                node_count,
                (int)index,
                state)) {
            return 1;
        }
    }

    return 0;
}

static void cycle_print_found(
    const char *title,
    path_node *nodes,
    cycle_search_state *state)
{
    unsigned int index;
    int start_position;

    (void)puts(title);
    (void)puts("----------------------------------------");

    start_position = -1;

    for (index = 0U;
         index < state->stack_used;
         ++index) {
        if (state->stack[index] ==
            state->cycle_start) {
            start_position = (int)index;
            break;
        }
    }

    if (start_position < 0) {
        (void)puts("Cycle detected, but path reconstruction failed.");
        return;
    }

    for (index = (unsigned int)start_position;
         index < state->stack_used;
         ++index) {
        (void)printf(
            "%s -> ",
            nodes[state->stack[index]].path
        );
    }

    (void)printf(
        "%s\n",
        nodes[state->cycle_start].path
    );
}

void symbol_cycle_all(agent_state *state)
{
    module_edge_list edges;
    path_node nodes[PATH_NODE_MAX];
    unsigned int node_count;
    cycle_search_state search;

    if (state == NULL ||
        state->project_root == NULL ||
        *state->project_root == '\0') {
        (void)puts("OVMS_AGENT_ROOT is not defined.");
        return;
    }

    if (!module_graph_current()) {
        (void)puts(
            "Symbol index is missing or stale; "
            "run REINDEX before CYCLE."
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

    if (!cycle_find_any(
            &edges,
            nodes,
            node_count,
            -1,
            &search)) {
        (void)puts("No project dependency cycle found.");
        return;
    }

    cycle_print_found(
        "Project dependency cycle",
        nodes,
        &search
    );
}

void symbol_cycle_module(agent_state *state,
                         const char *module)
{
    char path[SYMBOL_PATH_SIZE];
    module_edge_list edges;
    path_node nodes[PATH_NODE_MAX];
    unsigned int node_count;
    int start_index;
    cycle_search_state search;
    FILE *file;
    char title[SYMBOL_PATH_SIZE + 64U];

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
            "run REINDEX before CYCLE."
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

    start_index = path_node_find(
        nodes,
        node_count,
        path
    );

    if (start_index < 0) {
        (void)printf(
            "%s is not present in the module graph.\n",
            path
        );
        return;
    }

    if (!cycle_find_any(
            &edges,
            nodes,
            node_count,
            start_index,
            &search)) {
        (void)printf(
            "No dependency cycle reachable from %s.\n",
            path
        );
        return;
    }

    (void)snprintf(
        title,
        sizeof(title),
        "Dependency cycle reachable from %s",
        path
    );

    cycle_print_found(
        title,
        nodes,
        &search
    );
}

#define ARCH_TOP_MAX 10U
#define ARCH_SYMBOL_MAX 512U

typedef struct architecture_module_stat {
    char path[SYMBOL_PATH_SIZE];
    unsigned int fan_in;
    unsigned int fan_out;
} architecture_module_stat;

typedef struct architecture_symbol_stat {
    char name[SYMBOL_NAME_SIZE];
    unsigned int calls;
} architecture_symbol_stat;

static int architecture_module_find(
    architecture_module_stat *stats,
    unsigned int count,
    const char *path)
{
    unsigned int index;

    for (index = 0U; index < count; ++index) {
        if (strcmp(stats[index].path, path) == 0) {
            return (int)index;
        }
    }

    return -1;
}

static int architecture_module_add(
    architecture_module_stat *stats,
    unsigned int *count,
    const char *path)
{
    int existing;

    existing = architecture_module_find(
        stats,
        *count,
        path
    );

    if (existing >= 0) {
        return existing;
    }

    if (*count >= PATH_NODE_MAX ||
        strlen(path) >= SYMBOL_PATH_SIZE) {
        return -1;
    }

    (void)strcpy(stats[*count].path, path);
    stats[*count].fan_in = 0U;
    stats[*count].fan_out = 0U;

    ++(*count);
    return (int)(*count - 1U);
}

static void architecture_collect_module_stats(
    const module_edge_list *edges,
    architecture_module_stat *stats,
    unsigned int *count)
{
    unsigned int index;

    *count = 0U;

    for (index = 0U; index < edges->used; ++index) {
        int source_index;
        int target_index;

        source_index = architecture_module_add(
            stats,
            count,
            edges->edges[index].source
        );
        target_index = architecture_module_add(
            stats,
            count,
            edges->edges[index].target
        );

        if (source_index >= 0) {
            ++stats[source_index].fan_out;
        }

        if (target_index >= 0) {
            ++stats[target_index].fan_in;
        }
    }
}

static void architecture_sort_modules(
    architecture_module_stat *stats,
    unsigned int count,
    int sort_by_fan_in)
{
    unsigned int left;

    for (left = 0U; left < count; ++left) {
        unsigned int right;

        for (right = left + 1U;
             right < count;
             ++right) {
            unsigned int left_value;
            unsigned int right_value;

            left_value = sort_by_fan_in ?
                stats[left].fan_in :
                stats[left].fan_out;
            right_value = sort_by_fan_in ?
                stats[right].fan_in :
                stats[right].fan_out;

            if (right_value > left_value) {
                architecture_module_stat temporary;

                temporary = stats[left];
                stats[left] = stats[right];
                stats[right] = temporary;
            }
        }
    }
}

static int architecture_symbol_find(
    architecture_symbol_stat *stats,
    unsigned int count,
    const char *name)
{
    unsigned int index;

    for (index = 0U; index < count; ++index) {
        if (strcmp(stats[index].name, name) == 0) {
            return (int)index;
        }
    }

    return -1;
}

static void architecture_collect_symbol_stats(
    architecture_symbol_stat *stats,
    unsigned int *count)
{
    FILE *file;
    char line[SYMBOL_LINE_SIZE];

    *count = 0U;
    file = fopen(SYMBOL_INDEX_FILE, "r");

    if (file == NULL) {
        return;
    }

    while (fgets(line, sizeof(line), file) != NULL) {
        char kind;
        char *record_symbol;
        char *record_path;
        unsigned long record_line;
        int index;

        if (!symbol_parse_record(
                line,
                &kind,
                &record_symbol,
                &record_path,
                &record_line)) {
            continue;
        }

        if (kind != 'C') {
            continue;
        }

        index = architecture_symbol_find(
            stats,
            *count,
            record_symbol
        );

        if (index < 0) {
            if (*count >= ARCH_SYMBOL_MAX ||
                strlen(record_symbol) >= SYMBOL_NAME_SIZE) {
                continue;
            }

            (void)strcpy(
                stats[*count].name,
                record_symbol
            );
            stats[*count].calls = 0U;
            index = (int)(*count);
            ++(*count);
        }

        ++stats[index].calls;
    }

    (void)fclose(file);
}

static void architecture_sort_symbols(
    architecture_symbol_stat *stats,
    unsigned int count)
{
    unsigned int left;

    for (left = 0U; left < count; ++left) {
        unsigned int right;

        for (right = left + 1U;
             right < count;
             ++right) {
            if (stats[right].calls >
                stats[left].calls) {
                architecture_symbol_stat temporary;

                temporary = stats[left];
                stats[left] = stats[right];
                stats[right] = temporary;
            }
        }
    }
}

static void architecture_print_top_modules(
    const char *heading,
    architecture_module_stat *stats,
    unsigned int count,
    int fan_in)
{
    unsigned int limit;
    unsigned int index;

    architecture_sort_modules(
        stats,
        count,
        fan_in
    );

    limit = count < ARCH_TOP_MAX ?
        count : ARCH_TOP_MAX;

    (void)puts(heading);
    (void)puts("----------------------------------------");

    for (index = 0U; index < limit; ++index) {
        unsigned int value;

        value = fan_in ?
            stats[index].fan_in :
            stats[index].fan_out;

        (void)printf(
            "  %-36s %u\n",
            stats[index].path,
            value
        );
    }

    if (limit == 0U) {
        (void)puts("  none");
    }
}

static void architecture_print_top_symbols(
    architecture_symbol_stat *stats,
    unsigned int count)
{
    unsigned int limit;
    unsigned int index;

    architecture_sort_symbols(stats, count);

    limit = count < ARCH_TOP_MAX ?
        count : ARCH_TOP_MAX;

    (void)puts("Most widely used functions");
    (void)puts("----------------------------------------");

    for (index = 0U; index < limit; ++index) {
        (void)printf(
            "  %-36s %u calls\n",
            stats[index].name,
            stats[index].calls
        );
    }

    if (limit == 0U) {
        (void)puts("  none");
    }
}

void symbol_architecture(agent_state *state)
{
    module_edge_list edges;
    architecture_module_stat modules[PATH_NODE_MAX];
    architecture_module_stat fan_out_modules[PATH_NODE_MAX];
    architecture_symbol_stat symbols[ARCH_SYMBOL_MAX];
    cluster_state cluster;
    cluster_summary summaries[CLUSTER_MAX];
    unsigned int module_count;
    unsigned int symbol_count;
    unsigned int index;
    unsigned int multi_count;
    unsigned int singleton_count;
    cycle_search_state cycle_state;
    path_node nodes[PATH_NODE_MAX];
    unsigned int node_count;
    int has_cycle;

    if (state == NULL ||
        state->project_root == NULL ||
        *state->project_root == '\0') {
        (void)puts("OVMS_AGENT_ROOT is not defined.");
        return;
    }

    if (!module_graph_current()) {
        (void)puts(
            "Symbol index is missing or stale; "
            "run REINDEX before ARCHITECTURE."
        );
        return;
    }

    (void)memset(&edges, 0, sizeof(edges));
    (void)memset(modules, 0, sizeof(modules));
    (void)memset(fan_out_modules, 0, sizeof(fan_out_modules));
    (void)memset(symbols, 0, sizeof(symbols));
    (void)memset(summaries, 0, sizeof(summaries));
    (void)memset(nodes, 0, sizeof(nodes));

    if (!module_graph_collect_all(&edges)) {
        (void)puts("Unable to read the symbol index.");
        return;
    }

    architecture_collect_module_stats(
        &edges,
        modules,
        &module_count
    );
    (void)memcpy(
        fan_out_modules,
        modules,
        sizeof(modules)
    );

    architecture_collect_symbol_stats(
        symbols,
        &symbol_count
    );

    cluster_initialize(&cluster, &edges);
    cluster_run(&cluster, &edges);
    cluster_build_summaries(
        &cluster,
        summaries
    );

    multi_count = 0U;
    singleton_count = 0U;

    for (index = 0U;
         index < cluster.component_count;
         ++index) {
        if (summaries[index].size > 1U) {
            ++multi_count;
        } else {
            ++singleton_count;
        }
    }

    node_count = 0U;
    has_cycle = path_build_nodes(
                    &edges,
                    nodes,
                    &node_count) &&
                cycle_find_any(
                    &edges,
                    nodes,
                    node_count,
                    -1,
                    &cycle_state);

    (void)puts("Architecture report");
    (void)puts("========================================");
    (void)printf("Modules:                %u\n", module_count);
    (void)printf("Dependency edges:       %u\n", edges.used);
    (void)printf("Multi-module clusters:  %u\n", multi_count);
    (void)printf("Singleton modules:      %u\n", singleton_count);
    (void)printf("Dependency cycle found: %s\n",
                 has_cycle ? "yes" : "no");
    (void)puts("");

    architecture_print_top_modules(
        "Highest fan-in modules",
        modules,
        module_count,
        1
    );
    (void)puts("");

    architecture_print_top_modules(
        "Highest fan-out modules",
        fan_out_modules,
        module_count,
        0
    );
    (void)puts("");

    architecture_print_top_symbols(
        symbols,
        symbol_count
    );

    if (has_cycle) {
        (void)puts("");
        cycle_print_found(
            "Representative dependency cycle",
            nodes,
            &cycle_state
        );
    }
}

#define UNUSED_SYMBOL_MAX ARCH_SYMBOL_MAX

typedef struct unused_definition {
    char name[SYMBOL_NAME_SIZE];
    char path[SYMBOL_PATH_SIZE];
    unsigned long line;
    unsigned int calls;
} unused_definition;

static int unused_definition_find(
    unused_definition *definitions,
    unsigned int count,
    const char *name,
    const char *path,
    unsigned long line)
{
    unsigned int index;

    for (index = 0U; index < count; ++index) {
        if (strcmp(definitions[index].name, name) == 0 &&
            strcmp(definitions[index].path, path) == 0 &&
            definitions[index].line == line) {
            return (int)index;
        }
    }

    return -1;
}

static int unused_definition_add(
    unused_definition *definitions,
    unsigned int *count,
    const char *name,
    const char *path,
    unsigned long line)
{
    int existing;

    existing = unused_definition_find(
        definitions,
        *count,
        name,
        path,
        line
    );

    if (existing >= 0) {
        return existing;
    }

    if (*count >= UNUSED_SYMBOL_MAX ||
        strlen(name) >= SYMBOL_NAME_SIZE ||
        strlen(path) >= SYMBOL_PATH_SIZE) {
        return -1;
    }

    (void)strcpy(definitions[*count].name, name);
    (void)strcpy(definitions[*count].path, path);
    definitions[*count].line = line;
    definitions[*count].calls = 0U;

    ++(*count);
    return (int)(*count - 1U);
}

static int unused_symbol_is_entry_point(
    const char *name)
{
    return strcmp(name, "main") == 0 ||
           strcmp(name, "command_prompt") == 0 ||
           strcmp(name, "command_execute") == 0;
}

static int unused_symbol_is_callback_candidate(
    const char *name)
{
    return strncmp(name, "command_", 8U) == 0 ||
           strstr(name, "_handler") != NULL ||
           strstr(name, "_callback") != NULL;
}

static void unused_collect_definitions(
    unused_definition *definitions,
    unsigned int *count)
{
    FILE *file;
    char line[SYMBOL_LINE_SIZE];

    *count = 0U;
    file = fopen(SYMBOL_INDEX_FILE, "r");

    if (file == NULL) {
        return;
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

        if (kind == 'D') {
            (void)unused_definition_add(
                definitions,
                count,
                record_symbol,
                record_path,
                record_line
            );
        }
    }

    (void)fclose(file);
}

static void unused_count_calls(
    unused_definition *definitions,
    unsigned int definition_count)
{
    FILE *file;
    char line[SYMBOL_LINE_SIZE];

    file = fopen(SYMBOL_INDEX_FILE, "r");

    if (file == NULL) {
        return;
    }

    while (fgets(line, sizeof(line), file) != NULL) {
        char kind;
        char *record_symbol;
        char *record_path;
        unsigned long record_line;
        unsigned int index;

        if (!symbol_parse_record(
                line,
                &kind,
                &record_symbol,
                &record_path,
                &record_line)) {
            continue;
        }

        if (kind != 'C') {
            continue;
        }

        for (index = 0U;
             index < definition_count;
             ++index) {
            if (strcmp(
                    definitions[index].name,
                    record_symbol) == 0) {
                ++definitions[index].calls;
            }
        }
    }

    (void)fclose(file);
}

static void unused_collect_isolated_modules(
    const module_edge_list *edges,
    symbol_name_list *isolated)
{
    DIR *directory;
    struct dirent *entry;

    directory = opendir("SRC");

    if (directory == NULL) {
        return;
    }

    while ((entry = readdir(directory)) != NULL) {
        char path[SYMBOL_PATH_SIZE];
        unsigned int edge_index;
        int connected;

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

        connected = 0;

        for (edge_index = 0U;
             edge_index < edges->used;
             ++edge_index) {
            if (strcmp(
                    edges->edges[edge_index].source,
                    path) == 0 ||
                strcmp(
                    edges->edges[edge_index].target,
                    path) == 0) {
                connected = 1;
                break;
            }
        }

        if (!connected) {
            module_name_list_add(isolated, path);
        }
    }

    (void)closedir(directory);
}

void symbol_unused(agent_state *state)
{
    unused_definition definitions[UNUSED_SYMBOL_MAX];
    unsigned int definition_count;
    unsigned int index;
    unsigned int unused_count;
    unsigned int callback_count;
    module_edge_list edges;
    symbol_name_list isolated_modules;
    unsigned int checked;
    unsigned int changed;

    if (state == NULL ||
        state->project_root == NULL ||
        *state->project_root == '\0') {
        (void)puts("OVMS_AGENT_ROOT is not defined.");
        return;
    }

    if (!symbol_manifest_current(
            &checked,
            &changed,
            0)) {
        (void)puts(
            "Symbol index is missing or stale; "
            "run REINDEX before UNUSED."
        );
        return;
    }

    (void)memset(definitions, 0, sizeof(definitions));
    (void)memset(&edges, 0, sizeof(edges));
    (void)memset(
        &isolated_modules,
        0,
        sizeof(isolated_modules)
    );

    unused_collect_definitions(
        definitions,
        &definition_count
    );
    unused_count_calls(
        definitions,
        definition_count
    );

    (void)module_graph_collect_all(&edges);
    unused_collect_isolated_modules(
        &edges,
        &isolated_modules
    );

    unused_count = 0U;
    callback_count = 0U;

    (void)puts("Unused code analysis");
    (void)puts("========================================");

    (void)puts("Likely unused functions");
    (void)puts("----------------------------------------");

    for (index = 0U;
         index < definition_count;
         ++index) {
        if (definitions[index].calls != 0U ||
            unused_symbol_is_entry_point(
                definitions[index].name) ||
            unused_symbol_is_callback_candidate(
                definitions[index].name)) {
            continue;
        }

        (void)printf(
            "  %-32s %s line %lu\n",
            definitions[index].name,
            definitions[index].path,
            definitions[index].line
        );
        ++unused_count;
    }

    if (unused_count == 0U) {
        (void)puts("  none");
    }

    (void)puts("");
    (void)puts("Callback or registry candidates");
    (void)puts("----------------------------------------");

    for (index = 0U;
         index < definition_count;
         ++index) {
        if (definitions[index].calls == 0U &&
            unused_symbol_is_callback_candidate(
                definitions[index].name) &&
            !unused_symbol_is_entry_point(
                definitions[index].name)) {
            (void)printf(
                "  %-32s %s line %lu\n",
                definitions[index].name,
                definitions[index].path,
                definitions[index].line
            );
            ++callback_count;
        }
    }

    if (callback_count == 0U) {
        (void)puts("  none");
    }

    (void)puts("");
    module_print_list(
        "Isolated source modules",
        &isolated_modules
    );
    (void)puts("");

    (void)printf(
        "Summary: %u likely unused function%s, "
        "%u callback candidate%s, "
        "%u isolated module%s.\n",
        unused_count,
        unused_count == 1U ? "" : "s",
        callback_count,
        callback_count == 1U ? "" : "s",
        isolated_modules.used,
        isolated_modules.used == 1U ? "" : "s"
    );

    (void)puts(
        "Review all results before removing code; "
        "function pointers and external entry points may be invisible."
    );
}

typedef struct unused_detail_counts {
    unsigned int definitions;
    unsigned int calls;
    unsigned int declarations;
    unsigned int type_references;
    unsigned int ordinary_references;
    unsigned int table_references;
    unsigned int header_references;
} unused_detail_counts;

static int unused_detail_line_has_table_context(
    const char *line)
{
    return strchr(line, '{') != NULL ||
           strchr(line, '}') != NULL ||
           strchr(line, '[') != NULL ||
           strchr(line, ']') != NULL ||
           strstr(line, "registry") != NULL ||
           strstr(line, "table") != NULL;
}

static void unused_detail_print_match(
    const char *kind,
    const char *path,
    unsigned long line_number,
    const char *line)
{
    char display[SYMBOL_LINE_SIZE];
    size_t length;

    (void)strncpy(
        display,
        line,
        sizeof(display) - 1U
    );
    display[sizeof(display) - 1U] = '\0';

    length = strlen(display);

    while (length > 0U &&
           (display[length - 1U] == '\n' ||
            display[length - 1U] == '\r')) {
        display[--length] = '\0';
    }

    (void)printf(
        "%-5s %-32s line %-6lu %s\n",
        kind,
        path,
        line_number,
        display
    );
}

static void unused_detail_scan_sources(
    const char *symbol,
    unused_detail_counts *counts)
{
    DIR *directory;
    struct dirent *entry;

    directory = opendir("SRC");

    if (directory == NULL) {
        return;
    }

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
            (void)strncpy(
                clean,
                line,
                sizeof(clean) - 1U
            );
            clean[sizeof(clean) - 1U] = '\0';
            symbol_clean_line(
                clean,
                &in_block_comment
            );

            position = clean;

            while ((position = strstr(
                        position,
                        symbol)) != NULL) {
                int is_definition;
                int is_call;
                int is_type;
                int is_declaration;
                const char *kind;

                if (!symbol_match_at(
                        clean,
                        position,
                        symbol)) {
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
                is_type = usage_line_is_type_reference(
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

                if (!is_call &&
                    !is_declaration &&
                    !is_type) {
                    is_call = usage_line_is_call(
                        clean,
                        position,
                        symbol
                    );
                }

                if (is_definition) {
                    kind = "DEF";
                    ++counts->definitions;
                } else if (is_call) {
                    kind = "CALL";
                    ++counts->calls;
                } else if (is_type) {
                    kind = "TYPE";
                    ++counts->type_references;
                } else if (is_declaration) {
                    kind = "DECL";
                    ++counts->declarations;
                } else {
                    kind = "REF";
                    ++counts->ordinary_references;
                }

                if (symbol_has_header_extension(path)) {
                    ++counts->header_references;
                }

                if (!is_definition &&
                    unused_detail_line_has_table_context(
                        clean)) {
                    ++counts->table_references;
                }

                unused_detail_print_match(
                    kind,
                    path,
                    line_number,
                    line
                );

                position += strlen(symbol);
            }
        }

        (void)fclose(file);
    }

    (void)closedir(directory);
}

static void unused_detail_print_assessment(
    const char *symbol,
    const unused_detail_counts *counts)
{
    (void)puts("");
    (void)puts("Assessment");
    (void)puts("----------------------------------------");

    if (counts->definitions == 0U) {
        (void)puts(
            "No project definition was found."
        );
        return;
    }

    if (counts->calls > 0U) {
        (void)printf(
            "%s is not unused: %u direct indexed call%s found.\n",
            symbol,
            counts->calls,
            counts->calls == 1U ? "" : "s"
        );
        return;
    }

    if (counts->table_references > 0U) {
        (void)printf(
            "%s has no direct calls, but %u table or registry "
            "reference%s were found.\n",
            symbol,
            counts->table_references,
            counts->table_references == 1U ? "" : "s"
        );
        (void)puts(
            "This is likely a callback, command handler, "
            "or function-pointer target."
        );
        return;
    }

    if (counts->header_references > 0U ||
        counts->declarations > 0U) {
        (void)printf(
            "%s has no direct calls, but it is declared in a "
            "header or interface.\n",
            symbol
        );
        (void)puts(
            "It may be an externally callable project API."
        );
        return;
    }

    (void)printf(
        "%s has a definition but no direct calls, table references, "
        "or interface declarations.\n",
        symbol
    );
    (void)puts(
        "It is a strong unused-code candidate, but manual review "
        "is still required."
    );
}

void symbol_unused_detail(agent_state *state,
                          const char *symbol)
{
    unused_detail_counts counts;
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
            "run REINDEX before UNUSED/DETAIL."
        );
        return;
    }

    (void)memset(&counts, 0, sizeof(counts));

    (void)printf(
        "Unused-code detail for %s\n",
        symbol
    );
    (void)puts("========================================");

    unused_detail_scan_sources(
        symbol,
        &counts
    );

    (void)puts("");
    (void)puts("Reference summary");
    (void)puts("----------------------------------------");
    (void)printf(
        "Definitions:               %u\n",
        counts.definitions
    );
    (void)printf(
        "Direct calls:              %u\n",
        counts.calls
    );
    (void)printf(
        "Declarations:              %u\n",
        counts.declarations
    );
    (void)printf(
        "Type references:           %u\n",
        counts.type_references
    );
    (void)printf(
        "Ordinary references:       %u\n",
        counts.ordinary_references
    );
    (void)printf(
        "Table/registry references: %u\n",
        counts.table_references
    );
    (void)printf(
        "Header references:         %u\n",
        counts.header_references
    );

    unused_detail_print_assessment(
        symbol,
        &counts
    );
}

#define RENAME_MATCH_MAX 1024U

typedef struct rename_match {
    char path[SYMBOL_PATH_SIZE];
    unsigned long line;
    unsigned int occurrences;
} rename_match;

typedef struct rename_match_list {
    rename_match matches[RENAME_MATCH_MAX];
    unsigned int used;
    unsigned int total_occurrences;
} rename_match_list;

static int rename_identifier_valid(const char *name)
{
    const unsigned char *position;

    if (name == NULL ||
        *name == '\0' ||
        !(isalpha((unsigned char)*name) ||
          *name == '_')) {
        return 0;
    }

    position = (const unsigned char *)name + 1;

    while (*position != '\0') {
        if (!(isalnum(*position) ||
              *position == '_')) {
            return 0;
        }

        ++position;
    }

    return 1;
}

static unsigned int rename_count_line_matches(
    const char *line,
    const char *symbol)
{
    const char *position;
    unsigned int count;

    if (line == NULL || symbol == NULL) {
        return 0U;
    }

    position = line;
    count = 0U;

    while ((position = strstr(position, symbol)) != NULL) {
        if (symbol_match_at(line, position, symbol)) {
            ++count;
            position += strlen(symbol);
        } else {
            ++position;
        }
    }

    return count;
}

static void rename_match_add(
    rename_match_list *list,
    const char *path,
    unsigned long line,
    unsigned int occurrences)
{
    if (list == NULL ||
        path == NULL ||
        occurrences == 0U ||
        list->used >= RENAME_MATCH_MAX ||
        strlen(path) >= SYMBOL_PATH_SIZE) {
        return;
    }

    (void)strcpy(
        list->matches[list->used].path,
        path
    );
    list->matches[list->used].line = line;
    list->matches[list->used].occurrences = occurrences;
    ++list->used;
    list->total_occurrences += occurrences;
}

static void rename_collect_matches(
    const char *old_name,
    rename_match_list *list)
{
    DIR *directory;
    struct dirent *entry;

    directory = opendir("SRC");

    if (directory == NULL) {
        return;
    }

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
            unsigned int occurrences;

            ++line_number;
            (void)strncpy(
                clean,
                line,
                sizeof(clean) - 1U
            );
            clean[sizeof(clean) - 1U] = '\0';

            symbol_clean_line(
                clean,
                &in_block_comment
            );

            occurrences = rename_count_line_matches(
                clean,
                old_name
            );

            if (occurrences > 0U) {
                rename_match_add(
                    list,
                    path,
                    line_number,
                    occurrences
                );
            }
        }

        (void)fclose(file);
    }

    (void)closedir(directory);
}

static int rename_new_name_conflicts(
    const char *new_name)
{
    DIR *directory;
    struct dirent *entry;
    int found;

    directory = opendir("SRC");

    if (directory == NULL) {
        return 0;
    }

    found = 0;

    while (!found &&
           (entry = readdir(directory)) != NULL) {
        FILE *file;
        char path[SYMBOL_PATH_SIZE];
        char line[SYMBOL_LINE_SIZE];
        char clean[SYMBOL_LINE_SIZE];
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

        in_block_comment = 0;

        while (fgets(line, sizeof(line), file) != NULL) {
            (void)strncpy(
                clean,
                line,
                sizeof(clean) - 1U
            );
            clean[sizeof(clean) - 1U] = '\0';

            symbol_clean_line(
                clean,
                &in_block_comment
            );

            if (rename_count_line_matches(
                    clean,
                    new_name) > 0U) {
                found = 1;
                break;
            }
        }

        (void)fclose(file);
    }

    (void)closedir(directory);
    return found;
}

static void rename_print_preview(
    const rename_match_list *list,
    const char *old_name,
    const char *new_name)
{
    unsigned int index;

    (void)printf(
        "Rename preview: %s -> %s\n",
        old_name,
        new_name
    );
    (void)puts("========================================");

    if (list->used == 0U) {
        (void)puts("No exact identifier matches found.");
        return;
    }

    for (index = 0U;
         index < list->used;
         ++index) {
        (void)printf(
            "%-32s line %-6lu %u occurrence%s\n",
            list->matches[index].path,
            list->matches[index].line,
            list->matches[index].occurrences,
            list->matches[index].occurrences == 1U ? "" : "s"
        );
    }

    (void)puts("");
    (void)printf(
        "Files/lines affected: %u\n",
        list->used
    );
    (void)printf(
        "Total replacements:   %u\n",
        list->total_occurrences
    );
}

void symbol_rename_preview(agent_state *state,
                           const char *old_name,
                           const char *new_name)
{
    rename_match_list matches;
    unsigned int checked;
    unsigned int changed;

    if (state == NULL ||
        state->project_root == NULL ||
        *state->project_root == '\0') {
        (void)puts("OVMS_AGENT_ROOT is not defined.");
        return;
    }

    if (!rename_identifier_valid(old_name) ||
        !rename_identifier_valid(new_name)) {
        (void)puts(
            "Both names must be valid C identifiers."
        );
        return;
    }

    if (strcmp(old_name, new_name) == 0) {
        (void)puts(
            "Old and new names must be different."
        );
        return;
    }

    if (!symbol_manifest_current(
            &checked,
            &changed,
            0)) {
        (void)puts(
            "Symbol index is missing or stale; "
            "run REINDEX before RENAME."
        );
        return;
    }

    if (rename_new_name_conflicts(new_name)) {
        (void)printf(
            "Rename blocked: %s already exists in project source.\n",
            new_name
        );
        return;
    }

    (void)memset(&matches, 0, sizeof(matches));
    rename_collect_matches(
        old_name,
        &matches
    );

    rename_print_preview(
        &matches,
        old_name,
        new_name
    );

    if (matches.total_occurrences > 0U) {
        (void)puts("");
        (void)puts(
            "Preview only. No files were modified."
        );
        (void)puts(
            "Use the guarded write agent or PATCH commands "
            "to apply confirmed replacements."
        );
    }
}

#define RENAME_FILE_MAX 256U

typedef struct rename_file_change {
    char path[SYMBOL_PATH_SIZE];
    unsigned int replacements;
} rename_file_change;

typedef struct rename_transaction {
    char old_name[SYMBOL_NAME_SIZE];
    char new_name[SYMBOL_NAME_SIZE];
    rename_file_change files[RENAME_FILE_MAX];
    unsigned int file_count;
    unsigned int total_replacements;
} rename_transaction;

static int rename_transaction_find_file(
    const rename_transaction *transaction,
    const char *path)
{
    unsigned int index;

    for (index = 0U;
         index < transaction->file_count;
         ++index) {
        if (strcmp(
                transaction->files[index].path,
                path) == 0) {
            return (int)index;
        }
    }

    return -1;
}

static void rename_transaction_add(
    rename_transaction *transaction,
    const char *path,
    unsigned int replacements)
{
    int index;

    if (transaction == NULL ||
        path == NULL ||
        replacements == 0U) {
        return;
    }

    index = rename_transaction_find_file(
        transaction,
        path
    );

    if (index >= 0) {
        transaction->files[index].replacements +=
            replacements;
        transaction->total_replacements +=
            replacements;
        return;
    }

    if (transaction->file_count >= RENAME_FILE_MAX ||
        strlen(path) >= SYMBOL_PATH_SIZE) {
        return;
    }

    (void)strcpy(
        transaction->files[
            transaction->file_count
        ].path,
        path
    );
    transaction->files[
        transaction->file_count
    ].replacements = replacements;

    ++transaction->file_count;
    transaction->total_replacements +=
        replacements;
}

static void rename_build_transaction(
    const char *old_name,
    const char *new_name,
    rename_transaction *transaction)
{
    rename_match_list matches;
    unsigned int index;

    (void)memset(transaction, 0, sizeof(*transaction));
    (void)strcpy(transaction->old_name, old_name);
    (void)strcpy(transaction->new_name, new_name);

    (void)memset(&matches, 0, sizeof(matches));
    rename_collect_matches(old_name, &matches);

    for (index = 0U; index < matches.used; ++index) {
        rename_transaction_add(
            transaction,
            matches.matches[index].path,
            matches.matches[index].occurrences
        );
    }
}

static char *rename_replace_buffer(
    const char *input,
    const char *old_name,
    const char *new_name,
    unsigned int *replacement_count)
{
    const char *position;
    size_t input_length;
    size_t old_length;
    size_t new_length;
    size_t capacity;
    char *output;
    char *destination;
    unsigned int count;

    input_length = strlen(input);
    old_length = strlen(old_name);
    new_length = strlen(new_name);
    capacity = input_length + 1U;
    count = 0U;

    position = input;

    while ((position = strstr(position, old_name)) != NULL) {
        if (symbol_match_at(input, position, old_name)) {
            if (new_length > old_length) {
                capacity += new_length - old_length;
            }
            ++count;
            position += old_length;
        } else {
            ++position;
        }
    }

    output = (char *)malloc(capacity);

    if (output == NULL) {
        return NULL;
    }

    position = input;
    destination = output;

    while (*position != '\0') {
        const char *match;

        match = strstr(position, old_name);

        if (match == NULL) {
            size_t tail_length;

            tail_length = strlen(position);
            (void)memcpy(
                destination,
                position,
                tail_length
            );
            destination += tail_length;
            position += tail_length;
            break;
        }

        if (!symbol_match_at(input, match, old_name)) {
            size_t prefix;

            prefix = (size_t)(match - position) + 1U;
            (void)memcpy(destination, position, prefix);
            destination += prefix;
            position += prefix;
            continue;
        }

        {
            size_t prefix;

            prefix = (size_t)(match - position);
            (void)memcpy(destination, position, prefix);
            destination += prefix;

            (void)memcpy(
                destination,
                new_name,
                new_length
            );
            destination += new_length;
            position = match + old_length;
        }
    }

    *destination = '\0';

    if (replacement_count != NULL) {
        *replacement_count = count;
    }

    return output;
}

static int rename_apply_file(
    const char *path,
    const char *old_name,
    const char *new_name,
    unsigned int expected_replacements)
{
    FILE *file;
    char *input;
    char *output;
    long size;
    unsigned int actual_replacements;

    file = fopen(path, "rb");

    if (file == NULL) {
        return 0;
    }

    if (fseek(file, 0L, SEEK_END) != 0) {
        (void)fclose(file);
        return 0;
    }

    size = ftell(file);

    if (size < 0L ||
        fseek(file, 0L, SEEK_SET) != 0) {
        (void)fclose(file);
        return 0;
    }

    input = (char *)malloc((size_t)size + 1U);

    if (input == NULL) {
        (void)fclose(file);
        return 0;
    }

    if (size > 0L &&
        fread(input, 1U, (size_t)size, file) !=
            (size_t)size) {
        free(input);
        (void)fclose(file);
        return 0;
    }

    input[size] = '\0';
    (void)fclose(file);

    actual_replacements = 0U;
    output = rename_replace_buffer(
        input,
        old_name,
        new_name,
        &actual_replacements
    );
    free(input);

    if (output == NULL ||
        actual_replacements != expected_replacements) {
        free(output);
        return 0;
    }

    file = fopen(path, "w");

    if (file == NULL) {
        free(output);
        return 0;
    }

    if (fputs(output, file) == EOF ||
        fclose(file) != 0) {
        free(output);
        return 0;
    }

    free(output);
    return 1;
}

static int rename_confirm_transaction(
    const rename_transaction *transaction)
{
    char answer[32];

    (void)printf(
        "Rename %s -> %s\n",
        transaction->old_name,
        transaction->new_name
    );
    (void)puts("========================================");
    (void)printf(
        "Files affected:     %u\n",
        transaction->file_count
    );
    (void)printf(
        "Total replacements: %u\n",
        transaction->total_replacements
    );
    (void)puts("");

    {
        unsigned int index;

        for (index = 0U;
             index < transaction->file_count;
             ++index) {
            (void)printf(
                "  %-32s %u replacement%s\n",
                transaction->files[index].path,
                transaction->files[index].replacements,
                transaction->files[index].replacements == 1U ?
                    "" : "s"
            );
        }
    }

    (void)printf("Apply rename [y/N]? ");
    (void)fflush(stdout);

    if (fgets(answer, sizeof(answer), stdin) == NULL) {
        return 0;
    }

    return answer[0] == 'y' ||
           answer[0] == 'Y';
}

static int rename_confirm_rollback(void)
{
    char answer[32];

    (void)printf(
        "Restore previous OpenVMS file versions [Y/n]? "
    );
    (void)fflush(stdout);

    if (fgets(answer, sizeof(answer), stdin) == NULL) {
        return 1;
    }

    return answer[0] == '\n' ||
           answer[0] == '\r' ||
           answer[0] == 'y' ||
           answer[0] == 'Y';
}

static int rename_make_vms_filespec(
    const char *project_path,
    char *filespec,
    size_t filespec_size)
{
    const char *slash;
    const char *name;
    size_t directory_length;

    if (project_path == NULL ||
        filespec == NULL ||
        filespec_size == 0U) {
        return 0;
    }

    slash = strchr(project_path, '/');

    if (slash == NULL ||
        slash == project_path ||
        slash[1] == '\0') {
        return 0;
    }

    directory_length = (size_t)(slash - project_path);
    name = slash + 1;

    if ((size_t)snprintf(
            filespec,
            filespec_size,
            "[.%.*s]%s",
            (int)directory_length,
            project_path,
            name) >= filespec_size) {
        return 0;
    }

    return 1;
}

static int rename_restore_transaction(
    const rename_transaction *transaction)
{
    unsigned int index;
    int success;

    success = 1;

    for (index = 0U;
         index < transaction->file_count;
         ++index) {
        char filespec[SYMBOL_PATH_SIZE + 16U];
        char command[SYMBOL_PATH_SIZE * 2U + 96U];
        int status;

        if (!rename_make_vms_filespec(
                transaction->files[index].path,
                filespec,
                sizeof(filespec))) {
            success = 0;
            continue;
        }

        (void)snprintf(
            command,
            sizeof(command),
            "COPY/NOLOG %s;-1 %s;",
            filespec,
            filespec
        );

        status = system(command);

        if (status != 0 &&
            (status & 1) == 0) {
            success = 0;
        }
    }

    return success;
}

static int rename_apply_transaction(
    const rename_transaction *transaction)
{
    unsigned int index;

    for (index = 0U;
         index < transaction->file_count;
         ++index) {
        if (!rename_apply_file(
                transaction->files[index].path,
                transaction->old_name,
                transaction->new_name,
                transaction->files[index].replacements)) {
            return 0;
        }
    }

    return 1;
}

int symbol_rename_apply(agent_state *state,
                        const char *old_name,
                        const char *new_name)
{
    rename_transaction transaction;
    unsigned int checked;
    unsigned int changed;

    if (state == NULL ||
        state->project_root == NULL ||
        *state->project_root == '\0') {
        (void)puts("OVMS_AGENT_ROOT is not defined.");
        return 0;
    }

    if (!rename_identifier_valid(old_name) ||
        !rename_identifier_valid(new_name)) {
        (void)puts(
            "Both names must be valid C identifiers."
        );
        return 0;
    }

    if (strcmp(old_name, new_name) == 0) {
        (void)puts(
            "Old and new names must be different."
        );
        return 0;
    }

    if (!symbol_manifest_current(
            &checked,
            &changed,
            0)) {
        (void)puts(
            "Symbol index is missing or stale; "
            "run REINDEX before RENAME/APPLY."
        );
        return 0;
    }

    if (rename_new_name_conflicts(new_name)) {
        (void)printf(
            "Rename blocked: %s already exists in project source.\n",
            new_name
        );
        return 0;
    }

    rename_build_transaction(
        old_name,
        new_name,
        &transaction
    );

    if (transaction.total_replacements == 0U) {
        (void)puts("No exact identifier matches found.");
        return 0;
    }

    if (!rename_confirm_transaction(&transaction)) {
        (void)puts("Rename cancelled.");
        return 0;
    }

    if (!rename_apply_transaction(&transaction)) {
        (void)puts(
            "Rename failed while writing project files."
        );

        if (rename_confirm_rollback()) {
            if (rename_restore_transaction(&transaction)) {
                (void)puts(
                    "Previous file versions restored."
                );
            } else {
                (void)puts(
                    "Rollback was incomplete; inspect file versions."
                );
            }
        }

        return 0;
    }

    (void)puts(
        "Rename applied. The symbol index is now stale."
    );
    (void)puts(
        "Run REINDEX, then BUILD or AGENT/VERIFY."
    );
    return 1;
}

static int rename_run_reindex(agent_state *state)
{
    unsigned int checked;
    unsigned int changed;

    symbol_reindex(state);

    return symbol_manifest_current(
        &checked,
        &changed,
        0
    );
}

static int rename_run_controlled_build(void)
{
    int status;

    status = system(
        "@BUILD.COM"
    );

    /*
     * OpenVMS success statuses are odd. Preserve zero as success for
     * portability when this source is inspected or tested elsewhere.
     */
    return status == 0 ||
           (status & 1) != 0;
}


static int rename_force_recompile(
    const rename_transaction *transaction)
{
    unsigned int index;
    int success;

    success = 1;

    for (index = 0U;
         index < transaction->file_count;
         ++index) {
        const char *path;
        const char *name_start;
        const char *extension;
        char object_name[SYMBOL_NAME_SIZE];
        char command[SYMBOL_PATH_SIZE + SYMBOL_NAME_SIZE + 64U];
        size_t name_length;
        int status;

        path = transaction->files[index].path;
        extension = strrchr(path, '.');

        if (extension == NULL ||
            (strcmp(extension, ".c") != 0 &&
             strcmp(extension, ".C") != 0)) {
            continue;
        }

        name_start = strrchr(path, '/');

        if (name_start == NULL) {
            name_start = path;
        } else {
            ++name_start;
        }

        name_length = (size_t)(extension - name_start);

        if (name_length == 0U ||
            name_length >= sizeof(object_name)) {
            success = 0;
            continue;
        }

        (void)memcpy(
            object_name,
            name_start,
            name_length
        );
        object_name[name_length] = '\0';

        (void)snprintf(
            command,
            sizeof(command),
            "DELETE [.BUILD]%s.OBJ;*",
            object_name
        );

        status = system(command);

        /*
         * DELETE reports failure when no object exists. That is harmless:
         * the desired outcome is simply that no stale object remains.
         */
        (void)status;
    }

    return success;
}

static void rename_print_verify_result(
    int reindex_ok,
    int build_ok)
{
    (void)puts("");
    (void)puts("Rename verification");
    (void)puts("----------------------------------------");
    (void)printf(
        "Symbol index rebuild: %s\n",
        reindex_ok ? "PASS" : "FAIL"
    );
    (void)printf(
        "Controlled build:     %s\n",
        build_ok ? "PASS" : "FAIL"
    );
}

int symbol_rename_verify(agent_state *state,
                         const char *old_name,
                         const char *new_name)
{
    rename_transaction transaction;
    unsigned int checked;
    unsigned int changed;
    int reindex_ok;
    int build_ok;

    if (state == NULL ||
        state->project_root == NULL ||
        *state->project_root == '\0') {
        (void)puts("OVMS_AGENT_ROOT is not defined.");
        return 0;
    }

    if (!rename_identifier_valid(old_name) ||
        !rename_identifier_valid(new_name)) {
        (void)puts(
            "Both names must be valid C identifiers."
        );
        return 0;
    }

    if (strcmp(old_name, new_name) == 0) {
        (void)puts(
            "Old and new names must be different."
        );
        return 0;
    }

    if (!symbol_manifest_current(
            &checked,
            &changed,
            0)) {
        (void)puts(
            "Symbol index is missing or stale; "
            "run REINDEX before RENAME/VERIFY."
        );
        return 0;
    }

    if (rename_new_name_conflicts(new_name)) {
        (void)printf(
            "Rename blocked: %s already exists in project source.\n",
            new_name
        );
        return 0;
    }

    rename_build_transaction(
        old_name,
        new_name,
        &transaction
    );

    if (transaction.total_replacements == 0U) {
        (void)puts("No exact identifier matches found.");
        return 0;
    }

    if (!rename_confirm_transaction(&transaction)) {
        (void)puts("Rename cancelled.");
        return 0;
    }

    if (!rename_apply_transaction(&transaction)) {
        (void)puts(
            "Rename failed while writing project files."
        );

        if (rename_confirm_rollback()) {
            if (rename_restore_transaction(&transaction)) {
                (void)puts(
                    "Previous file versions restored."
                );
            } else {
                (void)puts(
                    "Rollback was incomplete; inspect file versions."
                );
            }
        }

        return 0;
    }

    (void)puts(
        "Rename applied. Rebuilding symbol index..."
    );
    reindex_ok = rename_run_reindex(state);

    (void)puts(
        "Invalidating changed object files..."
    );

    build_ok = reindex_ok &&
        rename_force_recompile(&transaction);

    (void)puts(
        "Running controlled build..."
    );
    build_ok = build_ok ?
        rename_run_controlled_build() : 0;

    rename_print_verify_result(
        reindex_ok,
        build_ok
    );

    if (reindex_ok && build_ok) {
        (void)puts(
            "Rename completed successfully."
        );
        return 1;
    }

    (void)puts(
        "Rename verification failed."
    );

    if (!rename_confirm_rollback()) {
        (void)puts(
            "Rollback declined. Project remains modified."
        );
        return 0;
    }

    if (!rename_restore_transaction(&transaction)) {
        (void)puts(
            "Rollback was incomplete; inspect file versions."
        );
        return 0;
    }

    (void)puts(
        "Previous file versions restored."
    );

    (void)puts(
        "Rebuilding symbol index after rollback..."
    );
    reindex_ok = rename_run_reindex(state);

    (void)puts(
        "Invalidating restored object files..."
    );

    build_ok = reindex_ok &&
        rename_force_recompile(&transaction);

    (void)puts(
        "Running controlled build after rollback..."
    );
    build_ok = build_ok ?
        rename_run_controlled_build() : 0;

    (void)puts("");
    (void)puts("Rollback verification");
    (void)puts("----------------------------------------");
    (void)printf(
        "Symbol index rebuild: %s\n",
        reindex_ok ? "PASS" : "FAIL"
    );
    (void)printf(
        "Controlled build:     %s\n",
        build_ok ? "PASS" : "FAIL"
    );

    if (reindex_ok && build_ok) {
        (void)puts(
            "Rollback completed successfully."
        );
    } else {
        (void)puts(
            "Rollback completed, but verification still failed."
        );
    }

    return 0;
}

typedef struct extract_preview_context {
    char module_path[SYMBOL_PATH_SIZE];
    char containing_function[SYMBOL_NAME_SIZE];
    unsigned long function_line;
    unsigned long start_line;
    unsigned long end_line;
    unsigned long selected_lines;
    unsigned long selected_bytes;
    unsigned int brace_delta;
    unsigned int has_return;
    unsigned int has_goto;
    unsigned int has_break;
    unsigned int has_continue;
    unsigned int has_label;
    unsigned int has_preprocessor;
} extract_preview_context;

static int extract_parse_range(
    const char *text,
    unsigned long *start_line,
    unsigned long *end_line)
{
    char *end_pointer;
    unsigned long start_value;
    unsigned long end_value;

    if (text == NULL ||
        start_line == NULL ||
        end_line == NULL) {
        return 0;
    }

    start_value = strtoul(text, &end_pointer, 10);

    if (end_pointer == text ||
        *end_pointer != ':') {
        return 0;
    }

    end_value = strtoul(
        end_pointer + 1,
        &end_pointer,
        10
    );

    if (*end_pointer != '\0' ||
        start_value == 0UL ||
        end_value < start_value) {
        return 0;
    }

    *start_line = start_value;
    *end_line = end_value;
    return 1;
}

static int extract_find_containing_function(
    const char *path,
    unsigned long start_line,
    char *name_out,
    size_t name_size,
    unsigned long *line_out)
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
            record_line > start_line ||
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
        strlen(best_name) >= name_size) {
        return 0;
    }

    (void)strcpy(name_out, best_name);
    *line_out = best_line;
    return 1;
}

static void extract_analyze_line(
    const char *line,
    extract_preview_context *context)
{
    const char *position;

    if (line == NULL || context == NULL) {
        return;
    }

    context->selected_bytes +=
        (unsigned long)strlen(line);

    for (position = line;
         *position != '\0';
         ++position) {
        if (*position == '{') {
            ++context->brace_delta;
        } else if (*position == '}' &&
                   context->brace_delta > 0U) {
            --context->brace_delta;
        }
    }

    if (strstr(line, "return") != NULL) {
        context->has_return = 1U;
    }

    if (strstr(line, "goto") != NULL) {
        context->has_goto = 1U;
    }

    if (strstr(line, "break") != NULL) {
        context->has_break = 1U;
    }

    if (strstr(line, "continue") != NULL) {
        context->has_continue = 1U;
    }

    {
        const char *trimmed;

        trimmed = line;

        while (*trimmed == ' ' || *trimmed == '\t') {
            ++trimmed;
        }

        if (*trimmed == '#') {
            context->has_preprocessor = 1U;
        }

        if ((isalpha((unsigned char)*trimmed) ||
             *trimmed == '_') &&
            strchr(trimmed, ':') != NULL &&
            strstr(trimmed, "case ") != trimmed &&
            strstr(trimmed, "default:") != trimmed) {
            context->has_label = 1U;
        }
    }
}

static int extract_print_selection(
    const char *path,
    unsigned long start_line,
    unsigned long end_line,
    extract_preview_context *context)
{
    FILE *file;
    char line[SYMBOL_LINE_SIZE];
    unsigned long line_number;

    file = fopen(path, "r");

    if (file == NULL) {
        return 0;
    }

    line_number = 0UL;

    while (fgets(line, sizeof(line), file) != NULL) {
        ++line_number;

        if (line_number < start_line) {
            continue;
        }

        if (line_number > end_line) {
            break;
        }

        (void)printf(
            "%6lu  %s",
            line_number,
            line
        );

        if (line[strlen(line) - 1U] != '\n') {
            (void)puts("");
        }

        extract_analyze_line(
            line,
            context
        );
        ++context->selected_lines;
    }

    (void)fclose(file);

    return context->selected_lines ==
           end_line - start_line + 1UL;
}

static void extract_print_risks(
    const extract_preview_context *context)
{
    (void)puts("");
    (void)puts("Extraction risks");
    (void)puts("----------------------------------------");

    if (!context->has_return &&
        !context->has_goto &&
        !context->has_break &&
        !context->has_continue &&
        context->brace_delta == 0U) {
        (void)puts("  No obvious control-flow blockers found.");
        return;
    }

    if (context->has_return) {
        (void)puts(
            "  Selection contains return; caller behavior must be preserved."
        );
    }

    if (context->has_goto) {
        (void)puts(
            "  Selection contains goto; labels may cross the extraction boundary."
        );
    }

    if (context->has_break) {
        (void)puts(
            "  Selection contains break; enclosing loop or switch may be required."
        );
    }

    if (context->has_continue) {
        (void)puts(
            "  Selection contains continue; enclosing loop may be required."
        );
    }

    if (context->brace_delta != 0U) {
        (void)puts(
            "  Selection has unbalanced braces and is not a complete block."
        );
    }
}


#define EXTRACT_IDENTIFIER_MAX 128U

typedef struct extract_identifier {
    char name[SYMBOL_NAME_SIZE];
    char inferred_type[64];
    unsigned int reads;
    unsigned int writes;
    unsigned int declarations;
    unsigned int function_calls;
    unsigned int pointer_writes;
    unsigned int pointer_value_writes;
    unsigned int constants;
    unsigned int address_taken;
    unsigned int pointee_reads;
} extract_identifier;

typedef struct extract_identifier_list {
    extract_identifier entries[EXTRACT_IDENTIFIER_MAX];
    unsigned int used;
} extract_identifier_list;

static int extract_exact_identifier_at(
    const char *line,
    const char *position,
    const char *identifier);

static int extract_entry_is_parameter(
    const extract_identifier *entry);


static int extract_identifier_is_keyword(const char *name)
{
    static const char *keywords[] = {
        "auto", "break", "case", "char", "const", "continue",
        "default", "do", "double", "else", "enum", "extern",
        "float", "for", "goto", "if", "inline", "int", "long",
        "register", "restrict", "return", "short", "signed",
        "sizeof", "static", "struct", "switch", "typedef",
        "union", "unsigned", "void", "volatile", "while", "_Bool",
        "_Complex", "_Imaginary", NULL
    };
    const char **keyword;

    for (keyword = keywords; *keyword != NULL; ++keyword) {
        if (strcmp(name, *keyword) == 0) {
            return 1;
        }
    }

    return 0;
}

static int extract_identifier_is_library_call(const char *name)
{
    return strcmp(name, "printf") == 0 ||
           strcmp(name, "puts") == 0 ||
           strcmp(name, "fputs") == 0 ||
           strcmp(name, "strlen") == 0 ||
           strcmp(name, "strcmp") == 0 ||
           strcmp(name, "memcpy") == 0 ||
           strcmp(name, "memset") == 0 ||
           strcmp(name, "malloc") == 0 ||
           strcmp(name, "free") == 0;
}

static int extract_identifier_find(
    extract_identifier_list *list,
    const char *name)
{
    unsigned int index;

    for (index = 0U; index < list->used; ++index) {
        if (strcmp(list->entries[index].name, name) == 0) {
            return (int)index;
        }
    }

    return -1;
}

static extract_identifier *extract_identifier_get(
    extract_identifier_list *list,
    const char *name)
{
    int index;

    index = extract_identifier_find(list, name);

    if (index >= 0) {
        return &list->entries[index];
    }

    if (list->used >= EXTRACT_IDENTIFIER_MAX ||
        strlen(name) >= SYMBOL_NAME_SIZE) {
        return NULL;
    }

    (void)strcpy(list->entries[list->used].name, name);
    list->entries[list->used].inferred_type[0] = '\0';
    list->entries[list->used].reads = 0U;
    list->entries[list->used].writes = 0U;
    list->entries[list->used].declarations = 0U;
    list->entries[list->used].function_calls = 0U;
    list->entries[list->used].pointer_writes = 0U;
    list->entries[list->used].pointer_value_writes = 0U;
    list->entries[list->used].constants = 0U;
    list->entries[list->used].address_taken = 0U;
    list->entries[list->used].pointee_reads = 0U;

    ++list->used;
    return &list->entries[list->used - 1U];
}

static int extract_line_declares_identifier(
    const char *line,
    const char *identifier)
{
    static const char *types[] = {
        "char", "short", "int", "long", "float", "double",
        "unsigned", "signed", "size_t", "FILE", "agent_state",
        "const", "struct", "enum", "union", NULL
    };
    const char **type_name;
    const char *position;

    position = strstr(line, identifier);

    if (position == NULL) {
        return 0;
    }

    for (type_name = types; *type_name != NULL; ++type_name) {
        const char *type_position;

        type_position = strstr(line, *type_name);

        if (type_position != NULL &&
            type_position < position) {
            return 1;
        }
    }

    return 0;
}


static int extract_identifier_is_constant(const char *name)
{
    const unsigned char *position;
    int has_letter;

    if (strcmp(name, "NULL") == 0 ||
        strcmp(name, "true") == 0 ||
        strcmp(name, "false") == 0) {
        return 1;
    }

    position = (const unsigned char *)name;
    has_letter = 0;

    while (*position != '\0') {
        if (isalpha(*position)) {
            has_letter = 1;
            if (islower(*position)) {
                return 0;
            }
        } else if (!(isdigit(*position) ||
                     *position == '_')) {
            return 0;
        }

        ++position;
    }

    return has_letter;
}

static int extract_identifier_is_function_call(
    const char *line,
    const char *identifier)
{
    const char *position;
    size_t length;

    length = strlen(identifier);
    position = line;

    while ((position = strstr(position, identifier)) != NULL) {
        const char *after;

        if (!symbol_match_at(line, position, identifier)) {
            ++position;
            continue;
        }

        after = position + length;

        while (*after == ' ' || *after == '\t') {
            ++after;
        }

        if (*after == '(') {
            return 1;
        }

        position += length;
    }

    return 0;
}


static int extract_line_reads_through_pointer(
    const char *line,
    const char *identifier)
{
    const char *position;
    size_t length;

    length = strlen(identifier);
    position = line;

    while ((position = strstr(position, identifier)) != NULL) {
        const char *before;
        const char *after;

        if (!extract_exact_identifier_at(
                line,
                position,
                identifier)) {
            ++position;
            continue;
        }

        before = position;

        while (before > line &&
               (before[-1] == ' ' || before[-1] == '\t')) {
            --before;
        }

        after = position + length;

        while (*after == ' ' || *after == '\t') {
            ++after;
        }

        if (before > line &&
            before[-1] == '*') {
            if (!(*after == '=' && after[1] != '=')) {
                return 1;
            }
        }

        position += length;
    }

    return 0;
}

static int extract_line_writes_through_pointer(
    const char *line,
    const char *identifier)
{
    const char *position;
    size_t length;

    length = strlen(identifier);
    position = line;

    while ((position = strstr(position, identifier)) != NULL) {
        const char *before;
        const char *after;

        if (!symbol_match_at(line, position, identifier)) {
            ++position;
            continue;
        }

        before = position;

        while (before > line &&
               (before[-1] == ' ' || before[-1] == '\t')) {
            --before;
        }

        after = position + length;

        while (*after == ' ' || *after == '\t') {
            ++after;
        }

        if (before > line &&
            before[-1] == '*' &&
            *after == '=' &&
            after[1] != '=') {
            return 1;
        }

        position += length;
    }

    return 0;
}

static int extract_line_writes_pointer_value(
    const char *line,
    const char *identifier)
{
    const char *position;
    size_t length;

    length = strlen(identifier);
    position = line;

    while ((position = strstr(position, identifier)) != NULL) {
        const char *after;

        if (!symbol_match_at(line, position, identifier)) {
            ++position;
            continue;
        }

        after = position + length;

        while (*after == ' ' || *after == '\t') {
            ++after;
        }

        if ((*after == '=' && after[1] != '=') ||
            (after[0] == '+' && after[1] == '+') ||
            (after[0] == '-' && after[1] == '-')) {
            return 1;
        }

        if (position >= line + 2 &&
            ((position[-2] == '+' && position[-1] == '+') ||
             (position[-2] == '-' && position[-1] == '-'))) {
            return 1;
        }

        position += length;
    }

    return 0;
}

static int extract_line_writes_identifier(
    const char *line,
    const char *identifier)
{
    const char *position;
    size_t length;

    length = strlen(identifier);
    position = line;

    while ((position = strstr(position, identifier)) != NULL) {
        const char *after;

        if (!symbol_match_at(line, position, identifier)) {
            ++position;
            continue;
        }

        after = position + length;

        while (*after == ' ' || *after == '\t') {
            ++after;
        }

        if (*after == '=' && after[1] != '=') {
            return 1;
        }

        if ((after[0] == '+' && after[1] == '+') ||
            (after[0] == '-' && after[1] == '-')) {
            return 1;
        }

        if (position >= line + 2 &&
            ((position[-2] == '+' && position[-1] == '+') ||
             (position[-2] == '-' && position[-1] == '-'))) {
            return 1;
        }

        position += length;
    }

    return 0;
}


static void extract_set_inferred_type(
    extract_identifier *entry,
    const char *type_name)
{
    if (entry == NULL ||
        type_name == NULL ||
        *type_name == '\0' ||
        entry->inferred_type[0] != '\0' ||
        strlen(type_name) >= sizeof(entry->inferred_type)) {
        return;
    }

    (void)strcpy(entry->inferred_type, type_name);
}

static void extract_infer_type_from_line(
    const char *line,
    const char *identifier,
    extract_identifier *entry)
{
    const char *position;
    const char *type_start;
    const char *type_end;
    char type_buffer[64];
    size_t length;

    if (line == NULL ||
        identifier == NULL ||
        entry == NULL ||
        entry->inferred_type[0] != '\0') {
        return;
    }

    position = strstr(line, identifier);

    if (position == NULL) {
        return;
    }

    type_start = line;

    while (*type_start == ' ' || *type_start == '\t') {
        ++type_start;
    }

    type_end = position;

    while (type_end > type_start &&
           (type_end[-1] == ' ' || type_end[-1] == '\t')) {
        --type_end;
    }

    if (type_end <= type_start) {
        return;
    }

    length = (size_t)(type_end - type_start);

    if (length >= sizeof(type_buffer)) {
        return;
    }

    (void)memcpy(type_buffer, type_start, length);
    type_buffer[length] = '\0';

    if (strstr(type_buffer, "if") != NULL ||
        strstr(type_buffer, "while") != NULL ||
        strstr(type_buffer, "for") != NULL ||
        strstr(type_buffer, "return") != NULL) {
        return;
    }

    extract_set_inferred_type(entry, type_buffer);
}

static int extract_identifier_address_taken(
    const char *line,
    const char *identifier)
{
    const char *position;

    position = line;

    while ((position = strstr(position, identifier)) != NULL) {
        const char *before;

        if (!symbol_match_at(line, position, identifier)) {
            ++position;
            continue;
        }

        before = position;

        while (before > line &&
               (before[-1] == ' ' || before[-1] == '\t')) {
            --before;
        }

        if (before > line && before[-1] == '&') {
            return 1;
        }

        position += strlen(identifier);
    }

    return 0;
}


static void extract_trim_type(char *type_name)
{
    char *start;
    char *end;

    if (type_name == NULL) {
        return;
    }

    start = type_name;

    while (*start == ' ' || *start == '\t') {
        ++start;
    }

    if (start != type_name) {
        (void)memmove(
            type_name,
            start,
            strlen(start) + 1U
        );
    }

    end = type_name + strlen(type_name);

    while (end > type_name &&
           (end[-1] == ' ' || end[-1] == '\t')) {
        --end;
    }

    *end = '\0';
}

static void extract_normalize_type_spacing(char *type_name)
{
    char normalized[64];
    const char *source;
    char *destination;
    int pending_space;

    if (type_name == NULL) {
        return;
    }

    source = type_name;
    destination = normalized;
    pending_space = 0;

    while (*source != '\0' &&
           (size_t)(destination - normalized) + 1U <
               sizeof(normalized)) {
        if (*source == ' ' || *source == '\t') {
            pending_space = 1;
            ++source;
            continue;
        }

        if (*source == '*') {
            if (destination > normalized &&
                destination[-1] == ' ') {
                --destination;
            }

            *destination++ = '*';
            pending_space = 0;
            ++source;
            continue;
        }

        if (pending_space &&
            destination > normalized &&
            destination[-1] != '*') {
            *destination++ = ' ';
        }

        *destination++ = *source++;
        pending_space = 0;
    }

    *destination = '\0';
    (void)strcpy(type_name, normalized);
}

static int extract_exact_identifier_at(
    const char *line,
    const char *position,
    const char *identifier)
{
    size_t length;
    unsigned char before;
    unsigned char after;

    if (line == NULL ||
        position == NULL ||
        identifier == NULL) {
        return 0;
    }

    length = strlen(identifier);

    if (strncmp(position, identifier, length) != 0) {
        return 0;
    }

    before = position > line ?
        (unsigned char)position[-1] : 0U;
    after = (unsigned char)position[length];

    if (position > line &&
        (isalnum(before) || before == '_')) {
        return 0;
    }

    if (isalnum(after) || after == '_') {
        return 0;
    }

    return 1;
}

static void extract_set_type_from_fragment(
    extract_identifier *entry,
    const char *fragment,
    const char *identifier)
{
    char type_buffer[64];
    const char *position;
    size_t length;

    if (entry == NULL ||
        fragment == NULL ||
        identifier == NULL ||
        entry->inferred_type[0] != '\0') {
        return;
    }

    position = fragment;

    while ((position = strstr(position, identifier)) != NULL) {
        if (extract_exact_identifier_at(
                fragment,
                position,
                identifier)) {
            break;
        }

        ++position;
    }

    if (position == NULL) {
        return;
    }

    length = (size_t)(position - fragment);

    while (length > 0U &&
           (fragment[length - 1U] == ' ' ||
            fragment[length - 1U] == '\t')) {
        --length;
    }

    while (length > 0U &&
           fragment[length - 1U] == '*') {
        --length;
    }

    if (length == 0U ||
        length >= sizeof(type_buffer)) {
        return;
    }

    (void)memcpy(type_buffer, fragment, length);
    type_buffer[length] = '\0';
    extract_trim_type(type_buffer);

    {
        const char *star_scan;
        unsigned int star_count;
        size_t used;

        star_scan = fragment;
        star_count = 0U;

        while (star_scan < position) {
            if (*star_scan == '*') {
                ++star_count;
            }

            ++star_scan;
        }

        used = strlen(type_buffer);

        while (star_count > 0U &&
               used + 1U < sizeof(type_buffer)) {
            type_buffer[used++] = '*';
            type_buffer[used] = '\0';
            --star_count;
        }
    }

    extract_normalize_type_spacing(type_buffer);
    extract_set_inferred_type(entry, type_buffer);
}

static void extract_recover_parameter_types(
    const char *path,
    unsigned long function_line,
    const char *function_name,
    extract_identifier_list *identifiers)
{
    FILE *file;
    char line[SYMBOL_LINE_SIZE];
    char signature[SYMBOL_LINE_SIZE * 4U];
    unsigned long line_number;
    int collecting;
    char *open_paren;
    char *close_paren;
    char *cursor;

    if (function_name == NULL ||
        *function_name == '\0') {
        return;
    }

    file = fopen(path, "r");

    if (file == NULL) {
        return;
    }

    signature[0] = '\0';
    line_number = 0UL;
    collecting = 0;

    while (fgets(line, sizeof(line), file) != NULL) {
        ++line_number;

        if (line_number < function_line) {
            continue;
        }

        if (!collecting) {
            if (strstr(line, function_name) == NULL) {
                continue;
            }

            collecting = 1;
        }

        if (strlen(signature) + strlen(line) + 1U <
            sizeof(signature)) {
            (void)strcat(signature, line);
        }

        if (strchr(line, ')') != NULL) {
            break;
        }
    }

    (void)fclose(file);

    open_paren = strchr(signature, '(');
    close_paren = strrchr(signature, ')');

    if (open_paren == NULL ||
        close_paren == NULL ||
        close_paren <= open_paren) {
        return;
    }

    *close_paren = '\0';
    cursor = open_paren + 1;

    while (*cursor != '\0') {
        char *comma;
        char fragment[128];
        size_t length;
        unsigned int index;

        comma = strchr(cursor, ',');

        if (comma == NULL) {
            length = strlen(cursor);
        } else {
            length = (size_t)(comma - cursor);
        }

        if (length >= sizeof(fragment)) {
            length = sizeof(fragment) - 1U;
        }

        (void)memcpy(fragment, cursor, length);
        fragment[length] = '\0';
        extract_trim_type(fragment);

        for (index = 0U;
             index < identifiers->used;
             ++index) {
            extract_set_type_from_fragment(
                &identifiers->entries[index],
                fragment,
                identifiers->entries[index].name
            );
        }

        if (comma == NULL) {
            break;
        }

        cursor = comma + 1;
    }
}

static void extract_recover_local_types(
    const char *path,
    unsigned long function_line,
    unsigned long selection_start,
    extract_identifier_list *identifiers)
{
    FILE *file;
    char line[SYMBOL_LINE_SIZE];
    unsigned long line_number;

    file = fopen(path, "r");

    if (file == NULL) {
        return;
    }

    line_number = 0UL;

    while (fgets(line, sizeof(line), file) != NULL) {
        unsigned int index;

        ++line_number;

        if (line_number <= function_line ||
            line_number >= selection_start) {
            continue;
        }

        for (index = 0U;
             index < identifiers->used;
             ++index) {
            extract_identifier *entry;

            entry = &identifiers->entries[index];

            if (entry->inferred_type[0] != '\0') {
                continue;
            }

            if (extract_line_declares_identifier(
                    line,
                    entry->name)) {
                extract_set_type_from_fragment(
                    entry,
                    line,
                    entry->name
                );
            }
        }
    }

    (void)fclose(file);
}

static int extract_type_is_pointer(const char *type_name)
{
    return type_name != NULL &&
           strchr(type_name, '*') != NULL;
}

static void extract_print_parameter(
    const char *type_name,
    const char *name,
    int add_pointer)
{
    char buffer[96];
    size_t length;

    (void)strncpy(
        buffer,
        type_name,
        sizeof(buffer) - 1U
    );
    buffer[sizeof(buffer) - 1U] = '\0';

    extract_trim_type(buffer);
    length = strlen(buffer);

    if (add_pointer &&
        length + 1U < sizeof(buffer)) {
        buffer[length++] = '*';
        buffer[length] = '\0';
    }

    extract_normalize_type_spacing(buffer);

    {
        char *first_star;

        first_star = strchr(buffer, '*');

        if (first_star != NULL) {
            size_t base_length;
            unsigned int star_count;
            const char *scan;

            base_length = (size_t)(first_star - buffer);

            while (base_length > 0U &&
                   buffer[base_length - 1U] == ' ') {
                --base_length;
            }

            star_count = 0U;

            for (scan = first_star;
                 *scan != '\0';
                 ++scan) {
                if (*scan == '*') {
                    ++star_count;
                }
            }

            (void)printf(
                "    %.*s ",
                (int)base_length,
                buffer
            );

            while (star_count > 0U) {
                (void)printf("*");
                --star_count;
            }

            (void)printf("%s", name);
        } else {
            (void)printf(
                "    %s %s",
                buffer,
                name
            );
        }
    }
}

static const char *extract_default_type(
    const extract_identifier *entry)
{
    if (entry->inferred_type[0] != '\0') {
        return entry->inferred_type;
    }

    if (entry->pointer_writes > 0U ||
        entry->pointer_value_writes > 0U) {
        return "char*";
    }

    return "int";
}

static void extract_print_signature(
    const char *new_name,
    const extract_identifier_list *identifiers)
{
    unsigned int index;
    unsigned int parameter_count;

    parameter_count = 0U;

    for (index = 0U; index < identifiers->used; ++index) {
        const extract_identifier *entry;

        entry = &identifiers->entries[index];

        if (entry->constants > 0U ||
            entry->function_calls > 0U ||
            entry->declarations > 0U) {
            continue;
        }

        ++parameter_count;
    }

    (void)puts("");
    (void)puts("Proposed function signature");
    (void)puts("----------------------------------------");

    (void)printf("static void %s(", new_name);

    if (parameter_count == 0U) {
        (void)puts("void)");
    } else {
        unsigned int emitted;

        emitted = 0U;
        (void)puts("");

        for (index = 0U; index < identifiers->used; ++index) {
            const extract_identifier *entry;
            const char *type_name;

            entry = &identifiers->entries[index];

            if (entry->constants > 0U ||
                entry->function_calls > 0U ||
                entry->declarations > 0U) {
                continue;
            }

            type_name = extract_default_type(entry);

            extract_print_parameter(
                type_name,
                entry->name,
                entry->writes > 0U ||
                entry->pointer_value_writes > 0U
            );

            ++emitted;

            if (emitted < parameter_count) {
                (void)puts(",");
            } else {
                (void)puts(")");
            }
        }
    }

    (void)puts("");
    (void)puts("Proposed replacement call");
    (void)puts("----------------------------------------");
    (void)printf("  %s(", new_name);

    if (parameter_count == 0U) {
        (void)puts(");");
    } else {
        unsigned int emitted;

        emitted = 0U;

        for (index = 0U; index < identifiers->used; ++index) {
            const extract_identifier *entry;

            entry = &identifiers->entries[index];

            if (entry->constants > 0U ||
                entry->function_calls > 0U ||
                entry->declarations > 0U) {
                continue;
            }

            if (entry->writes > 0U ||
                entry->pointer_value_writes > 0U) {
                (void)printf("&%s", entry->name);
            } else {
                (void)printf("%s", entry->name);
            }

            ++emitted;

            if (emitted < parameter_count) {
                (void)printf(", ");
            } else {
                (void)puts(");");
            }
        }
    }

    (void)puts(
        "Signature inference is heuristic and must be reviewed before application."
    );
}

static void extract_collect_identifiers_from_line(
    const char *line,
    extract_identifier_list *identifiers)
{
    const unsigned char *position;

    position = (const unsigned char *)line;

    while (*position != '\0') {
        char name[SYMBOL_NAME_SIZE];
        size_t used;
        extract_identifier *entry;
        int declared;
        int written;

        if (!(isalpha(*position) || *position == '_')) {
            ++position;
            continue;
        }

        used = 0U;

        while ((isalnum(*position) || *position == '_') &&
               used + 1U < sizeof(name)) {
            name[used++] = (char)*position++;
        }

        name[used] = '\0';

        if (extract_identifier_is_keyword(name) ||
            extract_identifier_is_library_call(name)) {
            continue;
        }

        entry = extract_identifier_get(identifiers, name);

        if (entry == NULL) {
            continue;
        }

        declared = extract_line_declares_identifier(line, name);
        written = extract_line_writes_identifier(line, name);

        if (extract_identifier_is_constant(name)) {
            ++entry->constants;
            continue;
        }

        if (extract_identifier_is_function_call(line, name)) {
            ++entry->function_calls;
            continue;
        }

        if (declared) {
            ++entry->declarations;
            extract_infer_type_from_line(
                line,
                name,
                entry
            );
        }

        if (extract_identifier_address_taken(line, name)) {
            ++entry->address_taken;
        }

        if (extract_line_reads_through_pointer(line, name)) {
            ++entry->pointee_reads;
        }

        if (extract_line_writes_through_pointer(line, name)) {
            ++entry->pointer_writes;
            ++entry->reads;
        } else if (extract_line_writes_pointer_value(line, name)) {
            ++entry->pointer_value_writes;
            ++entry->writes;
        } else if (written) {
            ++entry->writes;
        } else {
            ++entry->reads;
        }
    }
}

static void extract_analyze_selection_identifiers(
    const char *path,
    unsigned long start_line,
    unsigned long end_line,
    extract_identifier_list *identifiers)
{
    FILE *file;
    char line[SYMBOL_LINE_SIZE];
    char clean[SYMBOL_LINE_SIZE];
    unsigned long line_number;
    int in_block_comment;

    file = fopen(path, "r");

    if (file == NULL) {
        return;
    }

    line_number = 0UL;
    in_block_comment = 0;

    while (fgets(line, sizeof(line), file) != NULL) {
        ++line_number;

        if (line_number < start_line) {
            continue;
        }

        if (line_number > end_line) {
            break;
        }

        (void)strncpy(clean, line, sizeof(clean) - 1U);
        clean[sizeof(clean) - 1U] = '\0';
        symbol_clean_line(clean, &in_block_comment);

        extract_collect_identifiers_from_line(
            clean,
            identifiers
        );
    }

    (void)fclose(file);
}

static void extract_print_dataflow(
    const extract_identifier_list *identifiers)
{
    unsigned int index;
    unsigned int parameter_count;
    unsigned int output_count;
    unsigned int local_count;
    unsigned int function_count;
    unsigned int constant_count;

    parameter_count = 0U;
    output_count = 0U;
    local_count = 0U;
    function_count = 0U;
    constant_count = 0U;

    (void)puts("");
    (void)puts("Identifier-role analysis");
    (void)puts("----------------------------------------");

    for (index = 0U; index < identifiers->used; ++index) {
        const extract_identifier *entry;

        entry = &identifiers->entries[index];

        if (entry->constants > 0U) {
            ++constant_count;
            (void)printf(
                "  CONSTANT       %-20s uses=%u\n",
                entry->name,
                entry->constants
            );
        } else if (entry->function_calls > 0U) {
            ++function_count;
            (void)printf(
                "  FUNCTION CALL  %-20s calls=%u\n",
                entry->name,
                entry->function_calls
            );
        } else if (entry->declarations > 0U) {
            ++local_count;
            (void)printf(
                "  LOCAL          %-20s read=%u write=%u\n",
                entry->name,
                entry->reads,
                entry->writes
            );
        } else if (entry->pointer_writes > 0U &&
                   entry->pointer_value_writes == 0U) {
            ++parameter_count;
            (void)printf(
                "  INPUT POINTER  %-20s read=%u pointee-read=%u pointee-write=%u\n",
                entry->name,
                entry->reads,
                entry->pointee_reads,
                entry->pointer_writes
            );
        } else if (entry->writes > 0U ||
                   entry->pointer_value_writes > 0U) {
            ++output_count;
            (void)printf(
                "  INOUT          %-20s read=%u write=%u ptr-write=%u\n",
                entry->name,
                entry->reads,
                entry->writes,
                entry->pointer_value_writes
            );
        } else {
            ++parameter_count;
            (void)printf(
                "  INPUT          %-20s read=%u\n",
                entry->name,
                entry->reads
            );
        }
    }

    if (identifiers->used == 0U) {
        (void)puts("  none");
    }

    (void)puts("");
    (void)printf("Likely parameters:       %u\n", parameter_count);
    (void)printf("Likely output/inout:     %u\n", output_count);
    (void)printf("Locals declared inside:  %u\n", local_count);
    (void)printf("Function calls:          %u\n", function_count);
    (void)printf("Constants/macros:        %u\n", constant_count);
    (void)puts(
        "Heuristic only: typedef names, enum constants, macros, "
        "struct members, and aliases may still require review."
    );
}


typedef enum extract_eligibility {
    EXTRACT_ELIGIBILITY_SAFE = 1,
    EXTRACT_ELIGIBILITY_REVIEW = 2,
    EXTRACT_ELIGIBILITY_BLOCKED = 3
} extract_eligibility;

typedef struct extract_eligibility_result {
    extract_eligibility level;
    unsigned int unresolved_types;
    unsigned int outputs;
    unsigned int pointer_alias_risk;
} extract_eligibility_result;

static unsigned int extract_count_outputs(
    const extract_identifier_list *identifiers)
{
    unsigned int index;
    unsigned int count;

    count = 0U;

    for (index = 0U;
         index < identifiers->used;
         ++index) {
        const extract_identifier *entry;

        entry = &identifiers->entries[index];

        if (entry->constants > 0U ||
            entry->function_calls > 0U ||
            entry->declarations > 0U) {
            continue;
        }

        if (entry->writes > 0U ||
            entry->pointer_value_writes > 0U) {
            ++count;
        }
    }

    return count;
}

static unsigned int extract_count_unresolved_types(
    const extract_identifier_list *identifiers)
{
    unsigned int index;
    unsigned int count;

    count = 0U;

    for (index = 0U;
         index < identifiers->used;
         ++index) {
        const extract_identifier *entry;

        entry = &identifiers->entries[index];

        if (entry->constants > 0U ||
            entry->function_calls > 0U ||
            entry->declarations > 0U) {
            continue;
        }

        if (entry->inferred_type[0] == '\0') {
            ++count;
        }
    }

    return count;
}

static int extract_entry_is_output_slot(
    const extract_identifier *entry)
{
    return entry->pointer_value_writes > 0U ||
           (entry->writes > 0U &&
            entry->pointer_writes == 0U);
}

static int extract_entry_mutates_pointee(
    const extract_identifier *entry)
{
    return entry->pointer_writes > 0U;
}

static unsigned int extract_pointer_alias_risk(
    const extract_identifier_list *identifiers)
{
    unsigned int first;
    unsigned int second;

    for (first = 0U;
         first < identifiers->used;
         ++first) {
        const extract_identifier *left;
        const char *left_type;

        left = &identifiers->entries[first];

        if (!extract_entry_is_parameter(left)) {
            continue;
        }

        left_type = extract_default_type(left);

        if (strchr(left_type, '*') == NULL) {
            continue;
        }

        for (second = first + 1U;
             second < identifiers->used;
             ++second) {
            const extract_identifier *right;
            const char *right_type;
            int left_output_slot;
            int right_output_slot;
            int left_mutates_pointee;
            int right_mutates_pointee;

            right = &identifiers->entries[second];

            if (!extract_entry_is_parameter(right)) {
                continue;
            }

            right_type = extract_default_type(right);

            if (strchr(right_type, '*') == NULL) {
                continue;
            }

            left_output_slot =
                extract_entry_is_output_slot(left);
            right_output_slot =
                extract_entry_is_output_slot(right);
            left_mutates_pointee =
                extract_entry_mutates_pointee(left);
            right_mutates_pointee =
                extract_entry_mutates_pointee(right);

            /*
             * A read-only input pointer plus the address of a distinct local
             * pointer variable is not treated as an alias hazard. Example:
             *
             *     char *input;
             *     char *command;
             *     helper(input, &command);
             *
             * The second parameter is an output slot for the local variable,
             * not another pointer into the input buffer.
             */
            if ((left_output_slot &&
                 !left_mutates_pointee &&
                 !right_output_slot &&
                 !right_mutates_pointee) ||
                (right_output_slot &&
                 !right_mutates_pointee &&
                 !left_output_slot &&
                 !left_mutates_pointee)) {
                continue;
            }

            /*
             * Two pure output slots also refer to distinct caller variables
             * when generated as &left and &right.
             */
            if (left_output_slot &&
                right_output_slot &&
                !left_mutates_pointee &&
                !right_mutates_pointee) {
                continue;
            }

            /*
             * Retain review status when two parameters can both access or
             * mutate pointed-to storage.
             */
            if (left_mutates_pointee ||
                right_mutates_pointee ||
                left->pointee_reads > 0U ||
                right->pointee_reads > 0U) {
                return 1U;
            }
        }
    }

    return 0U;
}

static extract_eligibility_result extract_evaluate_eligibility(
    const extract_preview_context *context,
    const extract_identifier_list *identifiers)
{
    extract_eligibility_result result;

    result.level = EXTRACT_ELIGIBILITY_SAFE;
    result.unresolved_types =
        extract_count_unresolved_types(identifiers);
    result.outputs =
        extract_count_outputs(identifiers);
    result.pointer_alias_risk =
        extract_pointer_alias_risk(identifiers);

    if (context->has_goto ||
        context->has_break ||
        context->has_continue ||
        context->has_label ||
        context->has_preprocessor ||
        context->brace_delta != 0U) {
        result.level = EXTRACT_ELIGIBILITY_BLOCKED;
        return result;
    }

    if (context->has_return ||
        result.unresolved_types > 0U ||
        result.outputs > 1U ||
        result.pointer_alias_risk) {
        result.level = EXTRACT_ELIGIBILITY_REVIEW;
    }

    return result;
}

static const char *extract_eligibility_name(
    extract_eligibility level)
{
    switch (level) {
    case EXTRACT_ELIGIBILITY_SAFE:
        return "SAFE TO APPLY";

    case EXTRACT_ELIGIBILITY_REVIEW:
        return "REQUIRES REVIEW";

    case EXTRACT_ELIGIBILITY_BLOCKED:
        return "BLOCKED";

    default:
        return "UNKNOWN";
    }
}

static void extract_print_eligibility(
    const extract_preview_context *context,
    const extract_identifier_list *identifiers)
{
    extract_eligibility_result result;
    unsigned int reason_count;

    result = extract_evaluate_eligibility(
        context,
        identifiers
    );

    (void)puts("");
    (void)puts("Extraction eligibility");
    (void)puts("----------------------------------------");
    (void)printf(
        "  %s\n",
        extract_eligibility_name(result.level)
    );

    reason_count = 0U;

    if (context->has_return) {
        (void)puts(
            "  - selection contains return"
        );
        ++reason_count;
    }

    if (context->has_goto) {
        (void)puts(
            "  - selection contains goto"
        );
        ++reason_count;
    }

    if (context->has_break) {
        (void)puts(
            "  - selection contains break"
        );
        ++reason_count;
    }

    if (context->has_continue) {
        (void)puts(
            "  - selection contains continue"
        );
        ++reason_count;
    }

    if (context->has_label) {
        (void)puts(
            "  - selection contains a label"
        );
        ++reason_count;
    }

    if (context->has_preprocessor) {
        (void)puts(
            "  - selection contains a preprocessor directive"
        );
        ++reason_count;
    }

    if (context->brace_delta != 0U) {
        (void)puts(
            "  - selection has unbalanced braces"
        );
        ++reason_count;
    }

    if (result.unresolved_types > 0U) {
        (void)printf(
            "  - %u parameter type%s unresolved\n",
            result.unresolved_types,
            result.unresolved_types == 1U ? "" : "s"
        );
        ++reason_count;
    }

    if (result.outputs > 1U) {
        (void)printf(
            "  - %u output or in/out values require coordination\n",
            result.outputs
        );
        ++reason_count;
    }

    if (result.pointer_alias_risk) {
        (void)puts(
            "  - multiple parameters may access overlapping pointed-to storage"
        );
        ++reason_count;
    }

    if (reason_count == 0U) {
        (void)puts(
            "  - no blocking or review conditions detected"
        );
    }

    (void)puts(
        "Eligibility is heuristic; guarded application must still "
        "build and roll back on failure."
    );
}

void symbol_extract_function_preview(
    agent_state *state,
    const char *module,
    unsigned long start_line,
    unsigned long end_line,
    const char *new_name)
{
    extract_preview_context context;
    extract_identifier_list identifiers;
    FILE *file;
    unsigned int checked;
    unsigned int changed;

    if (state == NULL ||
        state->project_root == NULL ||
        *state->project_root == '\0') {
        (void)puts("OVMS_AGENT_ROOT is not defined.");
        return;
    }

    if (!rename_identifier_valid(new_name)) {
        (void)puts(
            "New function name must be a valid C identifier."
        );
        return;
    }

    if (!module_path_prepare(
            module,
            context.module_path,
            sizeof(context.module_path))) {
        (void)puts(
            "Module must be a project-relative .C file."
        );
        return;
    }

    if (!symbol_has_c_extension(context.module_path)) {
        (void)puts(
            "EXTRACT/FUNCTION requires a .C source module."
        );
        return;
    }

    if (start_line == 0UL ||
        end_line < start_line) {
        (void)puts(
            "Line range must use START:END with END >= START."
        );
        return;
    }

    if (!symbol_manifest_current(
            &checked,
            &changed,
            0)) {
        (void)puts(
            "Symbol index is missing or stale; "
            "run REINDEX before EXTRACT/FUNCTION."
        );
        return;
    }

    if (rename_new_name_conflicts(new_name)) {
        (void)printf(
            "Extraction blocked: %s already exists in project source.\n",
            new_name
        );
        return;
    }

    file = fopen(context.module_path, "r");

    if (file == NULL) {
        (void)printf(
            "Unable to open %s.\n",
            context.module_path
        );
        return;
    }

    (void)fclose(file);
    (void)memset(&context, 0, sizeof(context));
    (void)memset(&identifiers, 0, sizeof(identifiers));

    if (!module_path_prepare(
            module,
            context.module_path,
            sizeof(context.module_path))) {
        return;
    }

    context.start_line = start_line;
    context.end_line = end_line;

    (void)extract_find_containing_function(
        context.module_path,
        start_line,
        context.containing_function,
        sizeof(context.containing_function),
        &context.function_line
    );

    (void)printf(
        "Extract-function preview: %s %lu:%lu -> %s\n",
        context.module_path,
        start_line,
        end_line,
        new_name
    );
    (void)puts("========================================");

    if (context.containing_function[0] != '\0') {
        (void)printf(
            "Containing function: %s (line %lu)\n",
            context.containing_function,
            context.function_line
        );
    } else {
        (void)puts(
            "Containing function: not identified"
        );
    }

    (void)puts("");
    (void)puts("Selected source");
    (void)puts("----------------------------------------");

    if (!extract_print_selection(
            context.module_path,
            start_line,
            end_line,
            &context)) {
        (void)puts(
            "Selection extends beyond the end of the source file."
        );
        return;
    }

    (void)puts("");
    (void)puts("Planned transformation");
    (void)puts("----------------------------------------");
    (void)printf(
        "  Create static void %s(void)\n",
        new_name
    );
    (void)printf(
        "  Replace lines %lu:%lu with: %s();\n",
        start_line,
        end_line,
        new_name
    );
    (void)printf(
        "  Selected lines: %lu\n",
        context.selected_lines
    );
    (void)printf(
        "  Selected bytes: %lu\n",
        context.selected_bytes
    );

    extract_print_risks(&context);

    extract_analyze_selection_identifiers(
        context.module_path,
        start_line,
        end_line,
        &identifiers
    );

    extract_recover_parameter_types(
        context.module_path,
        context.function_line,
        context.containing_function,
        &identifiers
    );
    extract_recover_local_types(
        context.module_path,
        context.function_line,
        start_line,
        &identifiers
    );

    extract_print_dataflow(&identifiers);
    extract_print_signature(
        new_name,
        &identifiers
    );
    extract_print_eligibility(
        &context,
        &identifiers
    );

    (void)puts("");
    (void)puts(
        "Preview only. No files were modified."
    );
    (void)puts(
        "Parameter, local-variable, and return-value analysis "
        "will be required before guarded application."
    );
}

typedef struct extract_text_buffer {
    char *data;
    size_t used;
    size_t capacity;
} extract_text_buffer;

static int extract_buffer_reserve(
    extract_text_buffer *buffer,
    size_t additional)
{
    size_t required;
    size_t capacity;
    char *new_data;

    required = buffer->used + additional + 1U;

    if (required <= buffer->capacity) {
        return 1;
    }

    capacity = buffer->capacity == 0U ?
        4096U : buffer->capacity;

    while (capacity < required) {
        capacity *= 2U;
    }

    new_data = (char *)realloc(
        buffer->data,
        capacity
    );

    if (new_data == NULL) {
        return 0;
    }

    buffer->data = new_data;
    buffer->capacity = capacity;
    return 1;
}

static int extract_buffer_append_n(
    extract_text_buffer *buffer,
    const char *text,
    size_t length)
{
    if (!extract_buffer_reserve(buffer, length)) {
        return 0;
    }

    (void)memcpy(
        buffer->data + buffer->used,
        text,
        length
    );
    buffer->used += length;
    buffer->data[buffer->used] = '\0';
    return 1;
}

static int extract_buffer_append(
    extract_text_buffer *buffer,
    const char *text)
{
    return extract_buffer_append_n(
        buffer,
        text,
        strlen(text)
    );
}

static int extract_entry_is_parameter(
    const extract_identifier *entry)
{
    return entry->constants == 0U &&
           entry->function_calls == 0U &&
           entry->declarations == 0U;
}

static int extract_entry_pass_by_address(
    const extract_identifier *entry)
{
    return entry->writes > 0U ||
           entry->pointer_value_writes > 0U;
}

static int extract_append_parameter_decl(
    extract_text_buffer *buffer,
    const extract_identifier *entry)
{
    const char *type_name;
    char formatted[128];
    char type_buffer[96];
    char *first_star;
    size_t base_length;
    unsigned int star_count;
    const char *scan;

    type_name = extract_default_type(entry);

    (void)strncpy(
        type_buffer,
        type_name,
        sizeof(type_buffer) - 1U
    );
    type_buffer[sizeof(type_buffer) - 1U] = '\0';
    extract_trim_type(type_buffer);

    if (extract_entry_pass_by_address(entry)) {
        size_t length;

        length = strlen(type_buffer);

        if (length + 1U >= sizeof(type_buffer)) {
            return 0;
        }

        type_buffer[length] = '*';
        type_buffer[length + 1U] = '\0';
    }

    extract_normalize_type_spacing(type_buffer);
    first_star = strchr(type_buffer, '*');

    if (first_star == NULL) {
        (void)snprintf(
            formatted,
            sizeof(formatted),
            "%s %s",
            type_buffer,
            entry->name
        );
    } else {
        base_length = (size_t)(first_star - type_buffer);

        while (base_length > 0U &&
               type_buffer[base_length - 1U] == ' ') {
            --base_length;
        }

        star_count = 0U;

        for (scan = first_star;
             *scan != '\0';
             ++scan) {
            if (*scan == '*') {
                ++star_count;
            }
        }

        (void)snprintf(
            formatted,
            sizeof(formatted),
            "%.*s ",
            (int)base_length,
            type_buffer
        );

        while (star_count > 0U &&
               strlen(formatted) + 1U <
                   sizeof(formatted)) {
            (void)strcat(formatted, "*");
            --star_count;
        }

        if (strlen(formatted) +
            strlen(entry->name) <
            sizeof(formatted)) {
            (void)strcat(
                formatted,
                entry->name
            );
        }
    }

    return extract_buffer_append(
        buffer,
        formatted
    );
}

static int extract_append_signature_text(
    extract_text_buffer *buffer,
    const char *new_name,
    const extract_identifier_list *identifiers)
{
    unsigned int index;
    unsigned int count;
    unsigned int emitted;

    count = 0U;

    for (index = 0U;
         index < identifiers->used;
         ++index) {
        if (extract_entry_is_parameter(
                &identifiers->entries[index])) {
            ++count;
        }
    }

    if (!extract_buffer_append(buffer, "static void ") ||
        !extract_buffer_append(buffer, new_name) ||
        !extract_buffer_append(buffer, "(")) {
        return 0;
    }

    if (count == 0U) {
        return extract_buffer_append(
            buffer,
            "void)"
        );
    }

    if (!extract_buffer_append(buffer, "\n")) {
        return 0;
    }

    emitted = 0U;

    for (index = 0U;
         index < identifiers->used;
         ++index) {
        const extract_identifier *entry;

        entry = &identifiers->entries[index];

        if (!extract_entry_is_parameter(entry)) {
            continue;
        }

        if (!extract_buffer_append(buffer, "    ") ||
            !extract_append_parameter_decl(
                buffer,
                entry)) {
            return 0;
        }

        ++emitted;

        if (emitted < count) {
            if (!extract_buffer_append(buffer, ",\n")) {
                return 0;
            }
        } else if (!extract_buffer_append(buffer, ")")) {
            return 0;
        }
    }

    return 1;
}

static int extract_append_call_text(
    extract_text_buffer *buffer,
    const char *new_name,
    const extract_identifier_list *identifiers)
{
    unsigned int index;
    unsigned int count;
    unsigned int emitted;

    count = 0U;

    for (index = 0U;
         index < identifiers->used;
         ++index) {
        if (extract_entry_is_parameter(
                &identifiers->entries[index])) {
            ++count;
        }
    }

    if (!extract_buffer_append(buffer, new_name) ||
        !extract_buffer_append(buffer, "(")) {
        return 0;
    }

    emitted = 0U;

    for (index = 0U;
         index < identifiers->used;
         ++index) {
        const extract_identifier *entry;

        entry = &identifiers->entries[index];

        if (!extract_entry_is_parameter(entry)) {
            continue;
        }

        if (extract_entry_pass_by_address(entry) &&
            !extract_buffer_append(buffer, "&")) {
            return 0;
        }

        if (!extract_buffer_append(
                buffer,
                entry->name)) {
            return 0;
        }

        ++emitted;

        if (emitted < count &&
            !extract_buffer_append(buffer, ", ")) {
            return 0;
        }
    }

    return extract_buffer_append(buffer, ");");
}

static int extract_replace_identifier_for_body(
    const char *line,
    const extract_identifier_list *identifiers,
    extract_text_buffer *output)
{
    const char *position;

    position = line;

    while (*position != '\0') {
        unsigned int index;
        const extract_identifier *matched;
        size_t matched_length;

        matched = NULL;
        matched_length = 0U;

        for (index = 0U;
             index < identifiers->used;
             ++index) {
            const extract_identifier *entry;
            size_t length;

            entry = &identifiers->entries[index];

            if (!extract_entry_is_parameter(entry) ||
                !extract_entry_pass_by_address(entry)) {
                continue;
            }

            length = strlen(entry->name);

            if (extract_exact_identifier_at(
                    line,
                    position,
                    entry->name)) {
                matched = entry;
                matched_length = length;
                break;
            }
        }

        if (matched != NULL) {
            if (!extract_buffer_append(output, "*") ||
                !extract_buffer_append_n(
                    output,
                    position,
                    matched_length)) {
                return 0;
            }

            position += matched_length;
        } else {
            if (!extract_buffer_append_n(
                    output,
                    position,
                    1U)) {
                return 0;
            }

            ++position;
        }
    }

    return 1;
}

static unsigned int extract_line_indent(
    const char *line)
{
    unsigned int indent;

    indent = 0U;

    while (line[indent] == ' ' ||
           line[indent] == '\t') {
        ++indent;
    }

    return indent;
}

static int extract_build_helper(
    const char *path,
    unsigned long start_line,
    unsigned long end_line,
    const char *new_name,
    const extract_identifier_list *identifiers,
    extract_text_buffer *helper)
{
    FILE *file;
    char line[SYMBOL_LINE_SIZE];
    unsigned long line_number;
    unsigned int minimum_indent;
    int found_nonblank;

    file = fopen(path, "r");

    if (file == NULL) {
        return 0;
    }

    minimum_indent = 0U;
    found_nonblank = 0;
    line_number = 0UL;

    while (fgets(line, sizeof(line), file) != NULL) {
        unsigned int indent;
        const char *content;

        ++line_number;

        if (line_number < start_line ||
            line_number > end_line) {
            continue;
        }

        indent = extract_line_indent(line);
        content = line + indent;

        if (*content == '\0' || *content == '\n') {
            continue;
        }

        if (!found_nonblank ||
            indent < minimum_indent) {
            minimum_indent = indent;
            found_nonblank = 1;
        }
    }

    (void)fclose(file);

    if (!extract_append_signature_text(
            helper,
            new_name,
            identifiers) ||
        !extract_buffer_append(helper, "\n{\n")) {
        return 0;
    }

    file = fopen(path, "r");

    if (file == NULL) {
        return 0;
    }

    line_number = 0UL;

    while (fgets(line, sizeof(line), file) != NULL) {
        const char *content;
        unsigned int indent;

        ++line_number;

        if (line_number < start_line ||
            line_number > end_line) {
            continue;
        }

        indent = extract_line_indent(line);

        if (indent >= minimum_indent) {
            content = line + minimum_indent;
        } else {
            content = line;
        }

        if (!extract_buffer_append(helper, "    ") ||
            !extract_replace_identifier_for_body(
                content,
                identifiers,
                helper)) {
            (void)fclose(file);
            return 0;
        }
    }

    (void)fclose(file);

    return extract_buffer_append(
        helper,
        "}\n\n"
    );
}

static int extract_write_transformed_file(
    const char *path,
    unsigned long function_line,
    unsigned long start_line,
    unsigned long end_line,
    const char *new_name,
    const extract_identifier_list *identifiers)
{
    FILE *file;
    FILE *output;
    char line[SYMBOL_LINE_SIZE];
    unsigned long line_number;
    extract_text_buffer helper;
    extract_text_buffer call;
    unsigned int call_indent;

    (void)memset(&helper, 0, sizeof(helper));
    (void)memset(&call, 0, sizeof(call));

    if (!extract_build_helper(
            path,
            start_line,
            end_line,
            new_name,
            identifiers,
            &helper) ||
        !extract_append_call_text(
            &call,
            new_name,
            identifiers)) {
        free(helper.data);
        free(call.data);
        return 0;
    }

    file = fopen(path, "r");

    if (file == NULL) {
        free(helper.data);
        free(call.data);
        return 0;
    }

    output = fopen("OVMS_EXTRACT.TMP", "w");

    if (output == NULL) {
        (void)fclose(file);
        free(helper.data);
        free(call.data);
        return 0;
    }

    line_number = 0UL;
    call_indent = 0U;

    while (fgets(line, sizeof(line), file) != NULL) {
        ++line_number;

        if (line_number == function_line) {
            if (fputs(helper.data, output) == EOF) {
                goto write_failure;
            }
        }

        if (line_number == start_line) {
            unsigned int index;

            call_indent = extract_line_indent(line);

            for (index = 0U;
                 index < call_indent;
                 ++index) {
                if (fputc(
                        line[index],
                        output) == EOF) {
                    goto write_failure;
                }
            }

            if (fputs(call.data, output) == EOF ||
                fputc('\n', output) == EOF) {
                goto write_failure;
            }
        }

        if (line_number >= start_line &&
            line_number <= end_line) {
            continue;
        }

        if (fputs(line, output) == EOF) {
            goto write_failure;
        }
    }

    if (fclose(output) != 0) {
        output = NULL;
        goto close_failure;
    }

    output = NULL;
    (void)fclose(file);
    file = NULL;

    {
        char filespec[SYMBOL_PATH_SIZE + 16U];
        char command[SYMBOL_PATH_SIZE * 2U + 96U];
        int status;

        if (!rename_make_vms_filespec(
                path,
                filespec,
                sizeof(filespec))) {
            goto close_failure;
        }

        (void)snprintf(
            command,
            sizeof(command),
            "COPY/NOLOG OVMS_EXTRACT.TMP %s;",
            filespec
        );

        status = system(command);
        (void)remove("OVMS_EXTRACT.TMP");

        free(helper.data);
        free(call.data);

        return status == 0 ||
               (status & 1) != 0;
    }

write_failure:
    if (output != NULL) {
        (void)fclose(output);
        output = NULL;
    }

close_failure:
    if (file != NULL) {
        (void)fclose(file);
    }

    (void)remove("OVMS_EXTRACT.TMP");
    free(helper.data);
    free(call.data);
    return 0;
}

static int extract_confirm_apply(
    const extract_preview_context *context,
    const char *new_name,
    const extract_identifier_list *identifiers)
{
    char answer[32];
    extract_text_buffer signature;
    extract_text_buffer call;

    (void)memset(&signature, 0, sizeof(signature));
    (void)memset(&call, 0, sizeof(call));

    if (!extract_append_signature_text(
            &signature,
            new_name,
            identifiers) ||
        !extract_append_call_text(
            &call,
            new_name,
            identifiers)) {
        free(signature.data);
        free(call.data);
        return 0;
    }

    (void)puts("");
    (void)puts("Guarded extraction");
    (void)puts("----------------------------------------");
    (void)printf(
        "Module:    %s\n",
        context->module_path
    );
    (void)printf(
        "Range:     %lu:%lu\n",
        context->start_line,
        context->end_line
    );
    (void)printf(
        "Function:  %s\n",
        new_name
    );
    (void)puts("");
    (void)puts(signature.data);
    (void)printf("Call: %s\n", call.data);
    (void)printf("Apply extraction [y/N]? ");
    (void)fflush(stdout);

    free(signature.data);
    free(call.data);

    if (fgets(answer, sizeof(answer), stdin) == NULL) {
        return 0;
    }

    return answer[0] == 'y' ||
           answer[0] == 'Y';
}

int symbol_extract_function_apply(
    agent_state *state,
    const char *module,
    unsigned long start_line,
    unsigned long end_line,
    const char *new_name)
{
    extract_preview_context context;
    extract_identifier_list identifiers;
    extract_eligibility_result eligibility;
    rename_transaction transaction;
    unsigned int checked;
    unsigned int changed;
    int reindex_ok;
    int build_ok;

    (void)memset(&context, 0, sizeof(context));
    (void)memset(&identifiers, 0, sizeof(identifiers));
    (void)memset(&transaction, 0, sizeof(transaction));

    if (state == NULL ||
        state->project_root == NULL ||
        *state->project_root == '\0') {
        (void)puts("OVMS_AGENT_ROOT is not defined.");
        return 0;
    }

    if (!rename_identifier_valid(new_name)) {
        (void)puts(
            "New function name must be a valid C identifier."
        );
        return 0;
    }

    if (!module_path_prepare(
            module,
            context.module_path,
            sizeof(context.module_path)) ||
        !symbol_has_c_extension(
            context.module_path)) {
        (void)puts(
            "EXTRACT/FUNCTION/APPLY requires a "
            "project-relative .C source module."
        );
        return 0;
    }

    if (start_line == 0UL ||
        end_line < start_line) {
        (void)puts(
            "Line range must use START:END with END >= START."
        );
        return 0;
    }

    if (!symbol_manifest_current(
            &checked,
            &changed,
            0)) {
        (void)puts(
            "Symbol index is missing or stale; "
            "run REINDEX before EXTRACT/FUNCTION/APPLY."
        );
        return 0;
    }

    if (rename_new_name_conflicts(new_name)) {
        (void)printf(
            "Extraction blocked: %s already exists in project source.\n",
            new_name
        );
        return 0;
    }

    context.start_line = start_line;
    context.end_line = end_line;

    if (!extract_find_containing_function(
            context.module_path,
            start_line,
            context.containing_function,
            sizeof(context.containing_function),
            &context.function_line)) {
        (void)puts(
            "Unable to identify the containing function."
        );
        return 0;
    }

    if (!extract_print_selection(
            context.module_path,
            start_line,
            end_line,
            &context)) {
        (void)puts(
            "Selection extends beyond the end of the source file."
        );
        return 0;
    }

    extract_analyze_selection_identifiers(
        context.module_path,
        start_line,
        end_line,
        &identifiers
    );
    extract_recover_parameter_types(
        context.module_path,
        context.function_line,
        context.containing_function,
        &identifiers
    );
    extract_recover_local_types(
        context.module_path,
        context.function_line,
        start_line,
        &identifiers
    );

    eligibility = extract_evaluate_eligibility(
        &context,
        &identifiers
    );

    (void)printf(
        "Eligibility: %s\n",
        extract_eligibility_name(
            eligibility.level)
    );

    if (eligibility.level !=
        EXTRACT_ELIGIBILITY_SAFE) {
        (void)puts(
            "Extraction refused. Only SAFE TO APPLY "
            "selections may be modified."
        );
        return 0;
    }

    if (!extract_confirm_apply(
            &context,
            new_name,
            &identifiers)) {
        (void)puts("Extraction cancelled.");
        return 0;
    }

    (void)strcpy(
        transaction.files[0].path,
        context.module_path
    );
    transaction.files[0].replacements = 1U;
    transaction.file_count = 1U;
    transaction.total_replacements = 1U;

    if (!extract_write_transformed_file(
            context.module_path,
            context.function_line,
            start_line,
            end_line,
            new_name,
            &identifiers)) {
        (void)puts(
            "Extraction failed while writing the source file."
        );
        return 0;
    }

    (void)puts(
        "Extraction applied. Rebuilding symbol index..."
    );
    reindex_ok = rename_run_reindex(state);

    (void)puts(
        "Invalidating changed object files..."
    );
    build_ok = reindex_ok &&
        rename_force_recompile(&transaction);

    (void)puts(
        "Running controlled build..."
    );
    build_ok = build_ok ?
        rename_run_controlled_build() : 0;

    (void)puts("");
    (void)puts("Extraction verification");
    (void)puts("----------------------------------------");
    (void)printf(
        "Symbol index rebuild: %s\n",
        reindex_ok ? "PASS" : "FAIL"
    );
    (void)printf(
        "Controlled build:     %s\n",
        build_ok ? "PASS" : "FAIL"
    );

    if (reindex_ok && build_ok) {
        (void)puts(
            "Extraction completed successfully."
        );
        return 1;
    }

    (void)puts(
        "Extraction verification failed."
    );

    if (!rename_confirm_rollback()) {
        (void)puts(
            "Rollback declined. Project remains modified."
        );
        return 0;
    }

    if (!rename_restore_transaction(&transaction)) {
        (void)puts(
            "Rollback was incomplete; inspect file versions."
        );
        return 0;
    }

    (void)puts(
        "Previous source version restored."
    );
    reindex_ok = rename_run_reindex(state);
    build_ok = reindex_ok &&
        rename_force_recompile(&transaction);
    build_ok = build_ok ?
        rename_run_controlled_build() : 0;

    (void)puts("");
    (void)puts("Extraction rollback verification");
    (void)puts("----------------------------------------");
    (void)printf(
        "Symbol index rebuild: %s\n",
        reindex_ok ? "PASS" : "FAIL"
    );
    (void)printf(
        "Controlled build:     %s\n",
        build_ok ? "PASS" : "FAIL"
    );

    if (reindex_ok && build_ok) {
        (void)puts(
            "Extraction rollback completed successfully."
        );
    } else {
        (void)puts(
            "Rollback completed, but verification still failed."
        );
    }

    return 0;
}

typedef struct extract_semantic_check {
    unsigned int parameter_count;
    unsigned int input_count;
    unsigned int output_count;
    unsigned int local_count;
    unsigned int function_call_count;
    unsigned int constant_count;
    unsigned int unresolved_type_count;
    unsigned int address_parameter_count;
    unsigned int body_rewrite_count;
    unsigned int failures;
} extract_semantic_check;

static void extract_collect_semantic_counts(
    const extract_identifier_list *identifiers,
    extract_semantic_check *check)
{
    unsigned int index;

    (void)memset(check, 0, sizeof(*check));

    for (index = 0U;
         index < identifiers->used;
         ++index) {
        const extract_identifier *entry;

        entry = &identifiers->entries[index];

        if (entry->constants > 0U) {
            ++check->constant_count;
            continue;
        }

        if (entry->function_calls > 0U) {
            ++check->function_call_count;
            continue;
        }

        if (entry->declarations > 0U) {
            ++check->local_count;
            continue;
        }

        ++check->parameter_count;

        if (entry->inferred_type[0] == '\0') {
            ++check->unresolved_type_count;
        }

        if (extract_entry_pass_by_address(entry)) {
            ++check->output_count;
            ++check->address_parameter_count;
            ++check->body_rewrite_count;
        } else {
            ++check->input_count;
        }
    }
}

static int extract_verify_signature_consistency(
    const extract_identifier_list *identifiers,
    extract_semantic_check *check)
{
    unsigned int index;
    int success;

    success = 1;

    for (index = 0U;
         index < identifiers->used;
         ++index) {
        const extract_identifier *entry;
        const char *type_name;

        entry = &identifiers->entries[index];

        if (!extract_entry_is_parameter(entry)) {
            continue;
        }

        type_name = extract_default_type(entry);

        if (type_name == NULL ||
            *type_name == '\0') {
            ++check->failures;
            success = 0;
            continue;
        }

        if (extract_entry_pass_by_address(entry) &&
            entry->writes == 0U &&
            entry->pointer_value_writes == 0U) {
            ++check->failures;
            success = 0;
        }

        if (!extract_entry_pass_by_address(entry) &&
            (entry->writes > 0U ||
             entry->pointer_value_writes > 0U)) {
            ++check->failures;
            success = 0;
        }
    }

    return success;
}

static int extract_verify_generated_text(
    const char *new_name,
    const extract_identifier_list *identifiers,
    extract_semantic_check *check)
{
    extract_text_buffer signature;
    extract_text_buffer call;
    unsigned int index;
    int success;

    (void)memset(&signature, 0, sizeof(signature));
    (void)memset(&call, 0, sizeof(call));

    success =
        extract_append_signature_text(
            &signature,
            new_name,
            identifiers) &&
        extract_append_call_text(
            &call,
            new_name,
            identifiers);

    if (!success) {
        ++check->failures;
        free(signature.data);
        free(call.data);
        return 0;
    }

    for (index = 0U;
         index < identifiers->used;
         ++index) {
        const extract_identifier *entry;

        entry = &identifiers->entries[index];

        if (!extract_entry_is_parameter(entry)) {
            continue;
        }

        if (strstr(signature.data, entry->name) == NULL ||
            strstr(call.data, entry->name) == NULL) {
            ++check->failures;
            success = 0;
        }

        if (extract_entry_pass_by_address(entry)) {
            char address_text[SYMBOL_NAME_SIZE + 2U];

            (void)snprintf(
                address_text,
                sizeof(address_text),
                "&%s",
                entry->name
            );

            if (strstr(call.data, address_text) == NULL) {
                ++check->failures;
                success = 0;
            }
        }
    }

    free(signature.data);
    free(call.data);
    return success;
}

static int extract_verify_body_rewrite(
    const char *path,
    unsigned long start_line,
    unsigned long end_line,
    const extract_identifier_list *identifiers,
    extract_semantic_check *check)
{
    FILE *file;
    char line[SYMBOL_LINE_SIZE];
    unsigned long line_number;
    extract_text_buffer rewritten;
    unsigned int index;
    int success;

    (void)memset(&rewritten, 0, sizeof(rewritten));
    success = 1;

    file = fopen(path, "r");

    if (file == NULL) {
        ++check->failures;
        return 0;
    }

    line_number = 0UL;

    while (fgets(line, sizeof(line), file) != NULL) {
        ++line_number;

        if (line_number < start_line ||
            line_number > end_line) {
            continue;
        }

        if (!extract_replace_identifier_for_body(
                line,
                identifiers,
                &rewritten)) {
            success = 0;
            ++check->failures;
            break;
        }
    }

    (void)fclose(file);

    if (success) {
        for (index = 0U;
             index < identifiers->used;
             ++index) {
            const extract_identifier *entry;

            entry = &identifiers->entries[index];

            if (!extract_entry_is_parameter(entry) ||
                !extract_entry_pass_by_address(entry)) {
                continue;
            }

            {
                char dereference_text[SYMBOL_NAME_SIZE + 2U];

                (void)snprintf(
                    dereference_text,
                    sizeof(dereference_text),
                    "*%s",
                    entry->name
                );

                if (strstr(
                        rewritten.data,
                        dereference_text) == NULL) {
                    ++check->failures;
                    success = 0;
                }
            }
        }
    }

    free(rewritten.data);
    return success;
}

static void extract_print_semantic_check(
    const extract_preview_context *context,
    const extract_identifier_list *identifiers,
    const extract_semantic_check *check,
    int signature_ok,
    int generated_ok,
    int body_ok)
{
    (void)puts("");
    (void)puts("Semantic extraction verification");
    (void)puts("----------------------------------------");
    (void)printf(
        "Containing function:      %s\n",
        context->containing_function
    );
    (void)printf(
        "Parameters accounted for: %u\n",
        check->parameter_count
    );
    (void)printf(
        "Read-only inputs:         %u\n",
        check->input_count
    );
    (void)printf(
        "Output/in-out values:     %u\n",
        check->output_count
    );
    (void)printf(
        "Locals retained in body:  %u\n",
        check->local_count
    );
    (void)printf(
        "Function calls retained:  %u\n",
        check->function_call_count
    );
    (void)printf(
        "Constants retained:       %u\n",
        check->constant_count
    );
    (void)printf(
        "Types resolved:           %s\n",
        check->unresolved_type_count == 0U ?
            "PASS" : "FAIL"
    );
    (void)printf(
        "Signature consistency:    %s\n",
        signature_ok ? "PASS" : "FAIL"
    );
    (void)printf(
        "Generated call coverage:  %s\n",
        generated_ok ? "PASS" : "FAIL"
    );
    (void)printf(
        "Body rewrite coverage:    %s\n",
        body_ok ? "PASS" : "FAIL"
    );
    (void)printf(
        "Control-flow eligibility: %s\n",
        extract_eligibility_name(
            extract_evaluate_eligibility(
                context,
                identifiers).level)
    );
}

int symbol_extract_function_verify(
    agent_state *state,
    const char *module,
    unsigned long start_line,
    unsigned long end_line,
    const char *new_name)
{
    extract_preview_context context;
    extract_identifier_list identifiers;
    extract_semantic_check check;
    extract_eligibility_result eligibility;
    unsigned int checked;
    unsigned int changed;
    int signature_ok;
    int generated_ok;
    int body_ok;
    int success;

    (void)memset(&context, 0, sizeof(context));
    (void)memset(&identifiers, 0, sizeof(identifiers));

    if (state == NULL ||
        state->project_root == NULL ||
        *state->project_root == '\0') {
        (void)puts("OVMS_AGENT_ROOT is not defined.");
        return 0;
    }

    if (!rename_identifier_valid(new_name)) {
        (void)puts(
            "New function name must be a valid C identifier."
        );
        return 0;
    }

    if (!module_path_prepare(
            module,
            context.module_path,
            sizeof(context.module_path)) ||
        !symbol_has_c_extension(
            context.module_path)) {
        (void)puts(
            "EXTRACT/FUNCTION/VERIFY requires a "
            "project-relative .C source module."
        );
        return 0;
    }

    if (start_line == 0UL ||
        end_line < start_line) {
        (void)puts(
            "Line range must use START:END with END >= START."
        );
        return 0;
    }

    if (!symbol_manifest_current(
            &checked,
            &changed,
            0)) {
        (void)puts(
            "Symbol index is missing or stale; "
            "run REINDEX before EXTRACT/FUNCTION/VERIFY."
        );
        return 0;
    }

    if (rename_new_name_conflicts(new_name)) {
        (void)printf(
            "Verification blocked: %s already exists in project source.\n",
            new_name
        );
        return 0;
    }

    context.start_line = start_line;
    context.end_line = end_line;

    if (!extract_find_containing_function(
            context.module_path,
            start_line,
            context.containing_function,
            sizeof(context.containing_function),
            &context.function_line)) {
        (void)puts(
            "Unable to identify the containing function."
        );
        return 0;
    }

    if (!extract_print_selection(
            context.module_path,
            start_line,
            end_line,
            &context)) {
        (void)puts(
            "Selection extends beyond the end of the source file."
        );
        return 0;
    }

    extract_analyze_selection_identifiers(
        context.module_path,
        start_line,
        end_line,
        &identifiers
    );
    extract_recover_parameter_types(
        context.module_path,
        context.function_line,
        context.containing_function,
        &identifiers
    );
    extract_recover_local_types(
        context.module_path,
        context.function_line,
        start_line,
        &identifiers
    );

    extract_collect_semantic_counts(
        &identifiers,
        &check
    );

    signature_ok =
        extract_verify_signature_consistency(
            &identifiers,
            &check
        );
    generated_ok =
        extract_verify_generated_text(
            new_name,
            &identifiers,
            &check
        );
    body_ok =
        extract_verify_body_rewrite(
            context.module_path,
            start_line,
            end_line,
            &identifiers,
            &check
        );

    eligibility = extract_evaluate_eligibility(
        &context,
        &identifiers
    );

    success =
        check.unresolved_type_count == 0U &&
        signature_ok &&
        generated_ok &&
        body_ok &&
        eligibility.level !=
            EXTRACT_ELIGIBILITY_BLOCKED;

    extract_print_semantic_check(
        &context,
        &identifiers,
        &check,
        signature_ok,
        generated_ok,
        body_ok
    );

    (void)puts("");
    (void)printf(
        "Semantic verification result: %s\n",
        success ? "PASS" : "FAIL"
    );

    if (success &&
        eligibility.level ==
            EXTRACT_ELIGIBILITY_SAFE) {
        (void)puts(
            "Selection is eligible for guarded extraction."
        );
    } else if (success) {
        (void)puts(
            "Transformation is internally consistent, "
            "but still requires review."
        );
    } else {
        (void)puts(
            "Do not apply this extraction."
        );
    }

    return success;
}

typedef struct inline_function_info {
    char symbol[SYMBOL_NAME_SIZE];
    char module[SYMBOL_PATH_SIZE];
    unsigned long definition_line;
    unsigned long body_start;
    unsigned long body_end;
    unsigned int caller_count;
    char caller_module[SYMBOL_PATH_SIZE];
    unsigned long caller_line;
} inline_function_info;

static int inline_find_definition(
    const char *symbol,
    inline_function_info *info)
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
            (void)strncpy(
                info->module,
                record_path,
                sizeof(info->module) - 1U
            );
            info->module[
                sizeof(info->module) - 1U] = '\0';
            info->definition_line = record_line;
            (void)fclose(file);
            return 1;
        }
    }

    (void)fclose(file);
    return 0;
}

static void inline_count_callers(
    const char *symbol,
    inline_function_info *info)
{
    FILE *file;
    char line[SYMBOL_LINE_SIZE];

    file = fopen(SYMBOL_INDEX_FILE, "r");

    if (file == NULL) {
        return;
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
            strcmp(record_symbol, symbol) == 0) {
            ++info->caller_count;

            if (info->caller_count == 1U) {
                (void)strncpy(
                    info->caller_module,
                    record_path,
                    sizeof(info->caller_module) - 1U
                );
                info->caller_module[
                    sizeof(info->caller_module) - 1U] = '\0';
                info->caller_line = record_line;
            }
        }
    }

    (void)fclose(file);
}

static int inline_find_body_range(
    inline_function_info *info)
{
    FILE *file;
    char line[SYMBOL_LINE_SIZE];
    unsigned long line_number;
    int seen_open;
    long depth;

    file = fopen(info->module, "r");

    if (file == NULL) {
        return 0;
    }

    line_number = 0UL;
    seen_open = 0;
    depth = 0L;

    while (fgets(line, sizeof(line), file) != NULL) {
        const char *position;

        ++line_number;

        if (line_number < info->definition_line) {
            continue;
        }

        for (position = line;
             *position != '\0';
             ++position) {
            if (*position == '{') {
                ++depth;

                if (!seen_open) {
                    seen_open = 1;
                    info->body_start = line_number + 1UL;
                }
            } else if (*position == '}' && seen_open) {
                --depth;

                if (depth == 0L) {
                    info->body_end = line_number - 1UL;
                    (void)fclose(file);
                    return 1;
                }
            }
        }
    }

    (void)fclose(file);
    return 0;
}

static int inline_source_line(
    const char *path,
    unsigned long wanted,
    char *output,
    size_t output_size)
{
    FILE *file;
    char line[SYMBOL_LINE_SIZE];
    unsigned long line_number;

    file = fopen(path, "r");

    if (file == NULL) {
        return 0;
    }

    line_number = 0UL;

    while (fgets(line, sizeof(line), file) != NULL) {
        ++line_number;

        if (line_number == wanted) {
            (void)strncpy(
                output,
                line,
                output_size - 1U
            );
            output[output_size - 1U] = '\0';
            (void)fclose(file);
            return 1;
        }
    }

    (void)fclose(file);
    return 0;
}

static void inline_print_body(
    const inline_function_info *info)
{
    FILE *file;
    char line[SYMBOL_LINE_SIZE];
    unsigned long line_number;

    file = fopen(info->module, "r");

    if (file == NULL) {
        return;
    }

    line_number = 0UL;

    while (fgets(line, sizeof(line), file) != NULL) {
        ++line_number;

        if (line_number < info->body_start) {
            continue;
        }

        if (line_number > info->body_end) {
            break;
        }

        (void)printf(
            "%6lu  %s",
            line_number,
            line
        );

        if (line[0] != '\0' &&
            line[strlen(line) - 1U] != '\n') {
            (void)puts("");
        }
    }

    (void)fclose(file);
}

void symbol_inline_function_preview(
    agent_state *state,
    const char *symbol)
{
    inline_function_info info;
    unsigned int checked;
    unsigned int changed;
    char caller_text[SYMBOL_LINE_SIZE];

    (void)memset(&info, 0, sizeof(info));
    caller_text[0] = '\0';

    if (state == NULL ||
        state->project_root == NULL ||
        *state->project_root == '\0') {
        (void)puts("OVMS_AGENT_ROOT is not defined.");
        return;
    }

    if (!rename_identifier_valid(symbol)) {
        (void)puts(
            "Function name must be a valid C identifier."
        );
        return;
    }

    if (!symbol_manifest_current(
            &checked,
            &changed,
            0)) {
        (void)puts(
            "Symbol index is missing or stale; "
            "run REINDEX before INLINE/FUNCTION."
        );
        return;
    }

    (void)strncpy(
        info.symbol,
        symbol,
        sizeof(info.symbol) - 1U
    );

    if (!inline_find_definition(
            symbol,
            &info)) {
        (void)printf(
            "No definition found for %s.\n",
            symbol
        );
        return;
    }

    inline_count_callers(
        symbol,
        &info
    );

    if (!inline_find_body_range(&info)) {
        (void)puts(
            "Unable to identify a complete function body."
        );
        return;
    }

    (void)inline_source_line(
        info.caller_module,
        info.caller_line,
        caller_text,
        sizeof(caller_text)
    );

    (void)printf(
        "Inline-function preview: %s\n",
        symbol
    );
    (void)puts("========================================");
    (void)printf(
        "Definition: %s line %lu\n",
        info.module,
        info.definition_line
    );
    (void)printf(
        "Callers:    %u\n",
        info.caller_count
    );

    if (info.caller_count == 1U) {
        (void)printf(
            "Caller:     %s line %lu\n",
            info.caller_module,
            info.caller_line
        );
    }

    (void)puts("");
    (void)puts("Function body");
    (void)puts("----------------------------------------");
    inline_print_body(&info);

    if (info.caller_count == 1U) {
        (void)puts("");
        (void)puts("Call site");
        (void)puts("----------------------------------------");
        (void)printf(
            "%6lu  %s",
            info.caller_line,
            caller_text
        );

        if (caller_text[0] != '\0' &&
            caller_text[
                strlen(caller_text) - 1U] != '\n') {
            (void)puts("");
        }
    }

    (void)puts("");
    (void)puts("Inline eligibility");
    (void)puts("----------------------------------------");

    if (info.caller_count == 0U) {
        (void)puts(
            "  BLOCKED - function has no indexed callers"
        );
    } else if (info.caller_count > 1U) {
        (void)puts(
            "  REQUIRES REVIEW - function has multiple callers"
        );
    } else if (strcmp(
                   info.module,
                   info.caller_module) != 0) {
        (void)puts(
            "  REQUIRES REVIEW - caller is in a different module"
        );
    } else {
        (void)puts(
            "  SAFE TO PREVIEW - one caller in the same module"
        );
    }

    (void)puts("");
    (void)puts(
        "Preview only. No files were modified."
    );
    (void)puts(
        "Guarded application will require argument substitution, "
        "local-name collision checks, build verification, and rollback."
    );
}

#define INLINE_PARAMETER_MAX 16U

typedef struct inline_parameter {
    char name[SYMBOL_NAME_SIZE];
    char argument[SYMBOL_NAME_SIZE + 2U];
    unsigned int argument_is_address;
} inline_parameter;

typedef struct inline_parameter_list {
    inline_parameter entries[INLINE_PARAMETER_MAX];
    unsigned int used;
} inline_parameter_list;

static void inline_trim(char *text)
{
    char *start;
    char *end;

    if (text == NULL) {
        return;
    }

    start = text;

    while (*start == ' ' ||
           *start == '\t' ||
           *start == '\r' ||
           *start == '\n') {
        ++start;
    }

    if (start != text) {
        (void)memmove(
            text,
            start,
            strlen(start) + 1U
        );
    }

    end = text + strlen(text);

    while (end > text &&
           (end[-1] == ' ' ||
            end[-1] == '\t' ||
            end[-1] == '\r' ||
            end[-1] == '\n')) {
        --end;
    }

    *end = '\0';
}

static int inline_simple_argument(
    const char *text,
    char *name,
    size_t name_size,
    unsigned int *is_address)
{
    const char *position;
    size_t length;

    if (text == NULL ||
        name == NULL ||
        is_address == NULL) {
        return 0;
    }

    position = text;
    *is_address = 0U;

    while (*position == ' ' || *position == '\t') {
        ++position;
    }

    if (*position == '&') {
        *is_address = 1U;
        ++position;
    }

    while (*position == ' ' || *position == '\t') {
        ++position;
    }

    if (!(isalpha((unsigned char)*position) ||
          *position == '_')) {
        return 0;
    }

    length = 0U;

    while (isalnum((unsigned char)position[length]) ||
           position[length] == '_') {
        ++length;
    }

    if (length == 0U ||
        length >= name_size) {
        return 0;
    }

    {
        const char *tail;

        tail = position + length;

        while (*tail == ' ' || *tail == '\t') {
            ++tail;
        }

        if (*tail != '\0') {
            return 0;
        }
    }

    (void)memcpy(name, position, length);
    name[length] = '\0';
    return 1;
}

static int inline_extract_parameter_name(
    const char *fragment,
    char *name,
    size_t name_size)
{
    const char *end;
    const char *start;
    size_t length;

    if (fragment == NULL ||
        name == NULL ||
        name_size == 0U) {
        return 0;
    }

    end = fragment + strlen(fragment);

    while (end > fragment &&
           (end[-1] == ' ' ||
            end[-1] == '\t' ||
            end[-1] == '\r' ||
            end[-1] == '\n')) {
        --end;
    }

    start = end;

    while (start > fragment &&
           (isalnum((unsigned char)start[-1]) ||
            start[-1] == '_')) {
        --start;
    }

    length = (size_t)(end - start);

    if (length == 0U ||
        length >= name_size ||
        !(isalpha((unsigned char)*start) ||
          *start == '_')) {
        return 0;
    }

    (void)memcpy(name, start, length);
    name[length] = '\0';
    return 1;
}

static int inline_read_signature(
    const inline_function_info *info,
    char *parameters,
    size_t parameters_size)
{
    FILE *file;
    char line[SYMBOL_LINE_SIZE];
    char signature[SYMBOL_LINE_SIZE * 4U];
    unsigned long line_number;
    char *open_paren;
    char *close_paren;
    size_t length;

    file = fopen(info->module, "r");

    if (file == NULL) {
        return 0;
    }

    signature[0] = '\0';
    line_number = 0UL;

    while (fgets(line, sizeof(line), file) != NULL) {
        ++line_number;

        if (line_number < info->definition_line) {
            continue;
        }

        if (strlen(signature) + strlen(line) + 1U >=
            sizeof(signature)) {
            (void)fclose(file);
            return 0;
        }

        (void)strcat(signature, line);

        if (strchr(line, '{') != NULL) {
            break;
        }
    }

    (void)fclose(file);

    open_paren = strchr(signature, '(');
    close_paren = strrchr(signature, ')');

    if (open_paren == NULL ||
        close_paren == NULL ||
        close_paren <= open_paren) {
        return 0;
    }

    length = (size_t)(close_paren - open_paren - 1);

    if (length >= parameters_size) {
        return 0;
    }

    (void)memcpy(
        parameters,
        open_paren + 1,
        length
    );
    parameters[length] = '\0';
    inline_trim(parameters);
    return 1;
}

static int inline_read_call_arguments(
    const inline_function_info *info,
    char *arguments,
    size_t arguments_size)
{
    char line[SYMBOL_LINE_SIZE];
    char *symbol_position;
    char *open_paren;
    char *close_paren;
    size_t length;

    if (!inline_source_line(
            info->caller_module,
            info->caller_line,
            line,
            sizeof(line))) {
        return 0;
    }

    symbol_position = strstr(
        line,
        info->symbol
    );

    if (symbol_position == NULL) {
        return 0;
    }

    open_paren = strchr(symbol_position, '(');
    close_paren = strrchr(symbol_position, ')');

    if (open_paren == NULL ||
        close_paren == NULL ||
        close_paren <= open_paren) {
        return 0;
    }

    length = (size_t)(close_paren - open_paren - 1);

    if (length >= arguments_size) {
        return 0;
    }

    (void)memcpy(
        arguments,
        open_paren + 1,
        length
    );
    arguments[length] = '\0';
    inline_trim(arguments);
    return 1;
}

static int inline_parse_mapping(
    const inline_function_info *info,
    inline_parameter_list *mapping)
{
    char parameters[SYMBOL_LINE_SIZE * 2U];
    char arguments[SYMBOL_LINE_SIZE * 2U];
    char *parameter_cursor;
    char *argument_cursor;

    (void)memset(mapping, 0, sizeof(*mapping));

    if (!inline_read_signature(
            info,
            parameters,
            sizeof(parameters)) ||
        !inline_read_call_arguments(
            info,
            arguments,
            sizeof(arguments))) {
        return 0;
    }

    if (strcmp(parameters, "void") == 0) {
        parameters[0] = '\0';
    }

    parameter_cursor = parameters;
    argument_cursor = arguments;

    while (*parameter_cursor != '\0' ||
           *argument_cursor != '\0') {
        char *parameter_comma;
        char *argument_comma;
        char parameter_fragment[128];
        char argument_fragment[128];
        size_t parameter_length;
        size_t argument_length;
        inline_parameter *entry;

        if (mapping->used >= INLINE_PARAMETER_MAX) {
            return 0;
        }

        parameter_comma = strchr(parameter_cursor, ',');
        argument_comma = strchr(argument_cursor, ',');

        parameter_length = parameter_comma != NULL ?
            (size_t)(parameter_comma - parameter_cursor) :
            strlen(parameter_cursor);
        argument_length = argument_comma != NULL ?
            (size_t)(argument_comma - argument_cursor) :
            strlen(argument_cursor);

        if (parameter_length >= sizeof(parameter_fragment) ||
            argument_length >= sizeof(argument_fragment)) {
            return 0;
        }

        (void)memcpy(
            parameter_fragment,
            parameter_cursor,
            parameter_length
        );
        parameter_fragment[parameter_length] = '\0';
        inline_trim(parameter_fragment);

        (void)memcpy(
            argument_fragment,
            argument_cursor,
            argument_length
        );
        argument_fragment[argument_length] = '\0';
        inline_trim(argument_fragment);

        entry = &mapping->entries[mapping->used];

        if (!inline_extract_parameter_name(
                parameter_fragment,
                entry->name,
                sizeof(entry->name)) ||
            !inline_simple_argument(
                argument_fragment,
                entry->argument,
                sizeof(entry->argument),
                &entry->argument_is_address)) {
            return 0;
        }

        ++mapping->used;

        if ((parameter_comma == NULL) !=
            (argument_comma == NULL)) {
            return 0;
        }

        if (parameter_comma == NULL) {
            break;
        }

        parameter_cursor = parameter_comma + 1;
        argument_cursor = argument_comma + 1;
    }

    return 1;
}

static int inline_replace_body_line(
    const char *line,
    const inline_parameter_list *mapping,
    extract_text_buffer *output)
{
    const char *position;

    position = line;

    while (*position != '\0') {
        unsigned int index;
        const inline_parameter *matched;
        size_t matched_length;

        matched = NULL;
        matched_length = 0U;

        for (index = 0U;
             index < mapping->used;
             ++index) {
            const inline_parameter *entry;

            entry = &mapping->entries[index];

            if (extract_exact_identifier_at(
                    line,
                    position,
                    entry->name)) {
                matched = entry;
                matched_length = strlen(entry->name);
                break;
            }
        }

        if (matched == NULL) {
            if (!extract_buffer_append_n(
                    output,
                    position,
                    1U)) {
                return 0;
            }

            ++position;
            continue;
        }

        if (matched->argument_is_address &&
            output->used > 0U &&
            output->data[output->used - 1U] == '*') {
            --output->used;
            output->data[output->used] = '\0';
        }

        if (!extract_buffer_append(
                output,
                matched->argument)) {
            return 0;
        }

        position += matched_length;
    }

    return 1;
}

static int inline_build_expansion(
    const inline_function_info *info,
    const inline_parameter_list *mapping,
    extract_text_buffer *expansion)
{
    FILE *file;
    char line[SYMBOL_LINE_SIZE];
    unsigned long line_number;
    unsigned int minimum_indent;
    unsigned int caller_indent;
    int found_nonblank;
    char caller_line[SYMBOL_LINE_SIZE];

    if (!inline_source_line(
            info->caller_module,
            info->caller_line,
            caller_line,
            sizeof(caller_line))) {
        return 0;
    }

    caller_indent = extract_line_indent(caller_line);
    minimum_indent = 0U;
    found_nonblank = 0;

    file = fopen(info->module, "r");

    if (file == NULL) {
        return 0;
    }

    line_number = 0UL;

    while (fgets(line, sizeof(line), file) != NULL) {
        const char *content;
        unsigned int indent;

        ++line_number;

        if (line_number < info->body_start ||
            line_number > info->body_end) {
            continue;
        }

        indent = extract_line_indent(line);
        content = line + indent;

        if (*content == '\0' || *content == '\n') {
            continue;
        }

        if (!found_nonblank ||
            indent < minimum_indent) {
            minimum_indent = indent;
            found_nonblank = 1;
        }
    }

    (void)fclose(file);

    file = fopen(info->module, "r");

    if (file == NULL) {
        return 0;
    }

    line_number = 0UL;

    while (fgets(line, sizeof(line), file) != NULL) {
        const char *content;
        unsigned int indent;
        unsigned int index;

        ++line_number;

        if (line_number < info->body_start ||
            line_number > info->body_end) {
            continue;
        }

        indent = extract_line_indent(line);
        content = indent >= minimum_indent ?
            line + minimum_indent : line;

        for (index = 0U;
             index < caller_indent;
             ++index) {
            if (!extract_buffer_append_n(
                    expansion,
                    &caller_line[index],
                    1U)) {
                (void)fclose(file);
                return 0;
            }
        }

        if (!inline_replace_body_line(
                content,
                mapping,
                expansion)) {
            (void)fclose(file);
            return 0;
        }
    }

    (void)fclose(file);
    return 1;
}

static int inline_write_transformed_file(
    const inline_function_info *info,
    const extract_text_buffer *expansion)
{
    FILE *file;
    FILE *output;
    char line[SYMBOL_LINE_SIZE];
    unsigned long line_number;
    unsigned long definition_end;

    definition_end = info->body_end + 1UL;
    file = fopen(info->module, "r");

    if (file == NULL) {
        return 0;
    }

    output = fopen("OVMS_INLINE.TMP", "w");

    if (output == NULL) {
        (void)fclose(file);
        return 0;
    }

    line_number = 0UL;

    while (fgets(line, sizeof(line), file) != NULL) {
        ++line_number;

        if (line_number >= info->definition_line &&
            line_number <= definition_end) {
            continue;
        }

        if (line_number == info->caller_line) {
            if (fputs(expansion->data, output) == EOF) {
                goto inline_write_failure;
            }

            continue;
        }

        if (fputs(line, output) == EOF) {
            goto inline_write_failure;
        }
    }

    if (fclose(output) != 0) {
        output = NULL;
        goto inline_close_failure;
    }

    output = NULL;
    (void)fclose(file);
    file = NULL;

    {
        char filespec[SYMBOL_PATH_SIZE + 16U];
        char command[SYMBOL_PATH_SIZE * 2U + 96U];
        int status;

        if (!rename_make_vms_filespec(
                info->module,
                filespec,
                sizeof(filespec))) {
            goto inline_close_failure;
        }

        (void)snprintf(
            command,
            sizeof(command),
            "COPY/NOLOG OVMS_INLINE.TMP %s;",
            filespec
        );

        status = system(command);
        (void)remove("OVMS_INLINE.TMP");

        return status == 0 ||
               (status & 1) != 0;
    }

inline_write_failure:
    if (output != NULL) {
        (void)fclose(output);
        output = NULL;
    }

inline_close_failure:
    if (file != NULL) {
        (void)fclose(file);
    }

    (void)remove("OVMS_INLINE.TMP");
    return 0;
}

static int inline_confirm_apply(
    const inline_function_info *info,
    const extract_text_buffer *expansion)
{
    char answer[32];

    (void)puts("");
    (void)puts("Guarded inline");
    (void)puts("----------------------------------------");
    (void)printf("Function: %s\n", info->symbol);
    (void)printf(
        "Module:   %s\n",
        info->module
    );
    (void)printf(
        "Caller:   line %lu\n",
        info->caller_line
    );
    (void)puts("");
    (void)puts("Expanded body");
    (void)puts("----------------------------------------");
    (void)fputs(expansion->data, stdout);

    if (expansion->used > 0U &&
        expansion->data[expansion->used - 1U] != '\n') {
        (void)puts("");
    }

    (void)printf("Apply inline [y/N]? ");
    (void)fflush(stdout);

    if (fgets(answer, sizeof(answer), stdin) == NULL) {
        return 0;
    }

    return answer[0] == 'y' ||
           answer[0] == 'Y';
}

int symbol_inline_function_apply(
    agent_state *state,
    const char *symbol)
{
    inline_function_info info;
    inline_parameter_list mapping;
    extract_text_buffer expansion;
    rename_transaction transaction;
    unsigned int checked;
    unsigned int changed;
    int reindex_ok;
    int build_ok;

    (void)memset(&info, 0, sizeof(info));
    (void)memset(&mapping, 0, sizeof(mapping));
    (void)memset(&expansion, 0, sizeof(expansion));
    (void)memset(&transaction, 0, sizeof(transaction));

    if (state == NULL ||
        state->project_root == NULL ||
        *state->project_root == '\0') {
        (void)puts("OVMS_AGENT_ROOT is not defined.");
        return 0;
    }

    if (!rename_identifier_valid(symbol)) {
        (void)puts(
            "Function name must be a valid C identifier."
        );
        return 0;
    }

    if (!symbol_manifest_current(
            &checked,
            &changed,
            0)) {
        (void)puts(
            "Symbol index is missing or stale; "
            "run REINDEX before INLINE/FUNCTION/APPLY."
        );
        return 0;
    }

    (void)strncpy(
        info.symbol,
        symbol,
        sizeof(info.symbol) - 1U
    );

    if (!inline_find_definition(symbol, &info)) {
        (void)printf(
            "No definition found for %s.\n",
            symbol
        );
        return 0;
    }

    inline_count_callers(symbol, &info);

    if (info.caller_count != 1U) {
        (void)puts(
            "Inline refused. Exactly one indexed caller is required."
        );
        return 0;
    }

    if (strcmp(info.module, info.caller_module) != 0) {
        (void)puts(
            "Inline refused. The caller must be in the same module."
        );
        return 0;
    }

    if (!inline_find_body_range(&info)) {
        (void)puts(
            "Unable to identify a complete function body."
        );
        return 0;
    }

    if (!inline_parse_mapping(&info, &mapping)) {
        (void)puts(
            "Inline refused. Only simple identifier and "
            "&identifier call arguments are supported."
        );
        return 0;
    }

    if (!inline_build_expansion(
            &info,
            &mapping,
            &expansion)) {
        (void)puts(
            "Unable to generate the inline expansion."
        );
        free(expansion.data);
        return 0;
    }

    if (!inline_confirm_apply(
            &info,
            &expansion)) {
        (void)puts("Inline cancelled.");
        free(expansion.data);
        return 0;
    }

    (void)strcpy(
        transaction.files[0].path,
        info.module
    );
    transaction.files[0].replacements = 1U;
    transaction.file_count = 1U;
    transaction.total_replacements = 1U;

    if (!inline_write_transformed_file(
            &info,
            &expansion)) {
        (void)puts(
            "Inline failed while writing the source file."
        );
        free(expansion.data);
        return 0;
    }

    free(expansion.data);

    (void)puts(
        "Inline applied. Rebuilding symbol index..."
    );
    reindex_ok = rename_run_reindex(state);

    (void)puts(
        "Invalidating changed object files..."
    );
    build_ok = reindex_ok &&
        rename_force_recompile(&transaction);

    (void)puts(
        "Running controlled build..."
    );
    build_ok = build_ok ?
        rename_run_controlled_build() : 0;

    (void)puts("");
    (void)puts("Inline verification");
    (void)puts("----------------------------------------");
    (void)printf(
        "Symbol index rebuild: %s\n",
        reindex_ok ? "PASS" : "FAIL"
    );
    (void)printf(
        "Controlled build:     %s\n",
        build_ok ? "PASS" : "FAIL"
    );

    if (reindex_ok && build_ok) {
        (void)puts(
            "Inline completed successfully."
        );
        return 1;
    }

    (void)puts("Inline verification failed.");

    if (!rename_confirm_rollback()) {
        (void)puts(
            "Rollback declined. Project remains modified."
        );
        return 0;
    }

    if (!rename_restore_transaction(&transaction)) {
        (void)puts(
            "Rollback was incomplete; inspect file versions."
        );
        return 0;
    }

    (void)puts(
        "Previous source version restored."
    );
    reindex_ok = rename_run_reindex(state);
    build_ok = reindex_ok &&
        rename_force_recompile(&transaction);
    build_ok = build_ok ?
        rename_run_controlled_build() : 0;

    (void)puts("");
    (void)puts("Inline rollback verification");
    (void)puts("----------------------------------------");
    (void)printf(
        "Symbol index rebuild: %s\n",
        reindex_ok ? "PASS" : "FAIL"
    );
    (void)printf(
        "Controlled build:     %s\n",
        build_ok ? "PASS" : "FAIL"
    );

    if (reindex_ok && build_ok) {
        (void)puts(
            "Inline rollback completed successfully."
        );
    } else {
        (void)puts(
            "Rollback completed, but verification still failed."
        );
    }

    return 0;
}

