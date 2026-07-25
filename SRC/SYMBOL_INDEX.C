#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "symbol_index.h"

#define SYMBOL_LINE_SIZE 2048
#define SYMBOL_PATH_SIZE 512
#define SYMBOL_NAME_SIZE 256
#define SYMBOL_INDEX_VERSION 1

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
    const char *open_paren;
    const char *semicolon;
    const char *equal_sign;

    position = strstr(line, symbol);

    while (position != NULL) {
        if (symbol_match_at(line, position, symbol)) {
            after = position + strlen(symbol);

            while (*after != '\0' &&
                   isspace((unsigned char)*after)) {
                ++after;
            }

            if (*after == '(') {
                open_paren = after;
                semicolon = strchr(open_paren, ';');
                equal_sign = strchr(line, '=');

                if (equal_sign == NULL ||
                    equal_sign > position) {
                    if (semicolon == NULL) {
                        return 1;
                    }
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

static unsigned int symbol_index_line(FILE *index_file,
                                      const char *path,
                                      unsigned long line_number,
                                      char *line)
{
    char *position;
    unsigned int entries;

    entries = 0U;
    position = line;

    while (*position != '\0') {
        char symbol[SYMBOL_NAME_SIZE];
        char *after;
        size_t length;
        int definition;
        int call;

        while (*position != '\0' &&
               !(isalpha((unsigned char)*position) ||
                 *position == '_')) {
            ++position;
        }

        if (*position == '\0') {
            break;
        }

        after = position + 1;

        while (symbol_is_identifier_char(
                   (unsigned char)*after)) {
            ++after;
        }

        length = (size_t)(after - position);

        if (length >= sizeof(symbol)) {
            position = after;
            continue;
        }

        (void)memcpy(symbol, position, length);
        symbol[length] = '\0';
        position = after;

        if (symbol_is_keyword(symbol)) {
            continue;
        }

        while (*after != '\0' &&
               isspace((unsigned char)*after)) {
            ++after;
        }

        if (*after != '(') {
            continue;
        }

        definition = symbol_line_is_definition(line, symbol);
        call = !definition &&
               symbol_line_is_call(line, symbol);

        if (definition || call) {
            if (fprintf(
                    index_file,
                    "%c|%s|%s|%lu\n",
                    definition ? 'D' : 'C',
                    symbol,
                    path,
                    line_number) < 0) {
                return entries;
            }

            ++entries;
        }
    }

    return entries;
}

static unsigned int symbol_build_index(void)
{
    DIR *directory;
    struct dirent *entry;
    FILE *index_file;
    unsigned int files;
    unsigned int entries;

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

    (void)fprintf(
        index_file,
        "# OVMS Agent symbol index version %d\n",
        SYMBOL_INDEX_VERSION
    );

    files = 0U;
    entries = 0U;

    while ((entry = readdir(directory)) != NULL) {
        FILE *source_file;
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

        source_file = fopen(path, "r");

        if (source_file == NULL) {
            continue;
        }

        ++files;
        line_number = 0UL;
        in_block_comment = 0;

        while (fgets(line, sizeof(line), source_file) != NULL) {
            ++line_number;
            symbol_clean_line(line, &in_block_comment);
            entries += symbol_index_line(
                index_file,
                path,
                line_number,
                line
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

    file = fopen(SYMBOL_INDEX_FILE, "r");

    if (file != NULL) {
        (void)fclose(file);
        return symbol_query_index(
            symbol,
            show_definitions,
            show_calls
        );
    }

    (void)printf(
        "%s not found; using live source scan.\n",
        SYMBOL_INDEX_FILE
    );

    return symbol_scan_live(
        symbol,
        show_definitions,
        show_calls
    );
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

void symbol_index_status(agent_state *state)
{
    FILE *file;
    char line[SYMBOL_LINE_SIZE];
    unsigned int definitions;
    unsigned int calls;

    if (state == NULL ||
        state->project_root == NULL ||
        *state->project_root == '\0') {
        (void)puts("OVMS_AGENT_ROOT is not defined.");
        return;
    }

    file = fopen(SYMBOL_INDEX_FILE, "r");

    if (file == NULL) {
        (void)printf(
            "Symbol index: missing (%s)\n",
            SYMBOL_INDEX_FILE
        );
        (void)puts("Run REINDEX to create it.");
        return;
    }

    definitions = 0U;
    calls = 0U;

    while (fgets(line, sizeof(line), file) != NULL) {
        if (line[0] == 'D' && line[1] == '|') {
            ++definitions;
        } else if (line[0] == 'C' && line[1] == '|') {
            ++calls;
        }
    }

    (void)fclose(file);

    (void)printf("Symbol index: %s\n", SYMBOL_INDEX_FILE);
    (void)printf("Definitions:  %u\n", definitions);
    (void)printf("Call sites:   %u\n", calls);
    (void)printf(
        "Total entries: %u\n",
        definitions + calls
    );
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
