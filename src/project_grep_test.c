#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "agent.h"
#include "project.h"

#define TEST_BUFFER_SIZE 32768U

static void remove_all(const char *path)
{
    if (path == NULL) {
        return;
    }

    for (;;) {
        if (remove(path) != 0) {
            break;
        }
    }
}

static void cleanup_files(void)
{
    remove_all("SRC/M188_GREP_A.C");
    remove_all("DOC/M188_GREP_B.MD");
    remove_all("TEST/M188_GREP_C.TXT");
    remove_all("TEST/M188_GREP_LIMIT.TXT");
    remove_all("M188_GREP_SKIP.C");
    remove_all("M188_GREP_BASIC.OUT");
    remove_all("M188_GREP_LIMIT.OUT");
    remove_all("M189_GREP_PATH.OUT");
    remove_all("TEST/M190_GREP_CONTEXT.TXT");
    remove_all("M190_GREP_CONTEXT.OUT");
    remove_all("M190_GREP_ZERO.OUT");
    remove_all("M190_GREP_MAX.OUT");
}

static int write_text(const char *path,
                      const char *text)
{
    FILE *file;
    size_t length;

    if (path == NULL || text == NULL) {
        return 0;
    }

    file = fopen(path, "w");
    if (file == NULL) {
        return 0;
    }

    length = strlen(text);

    if (fwrite(text, 1U, length, file) != length) {
        (void)fclose(file);
        return 0;
    }

    if (fclose(file) != 0) {
        return 0;
    }

    return 1;
}

static int write_limit_file(const char *path,
                            const char *pattern)
{
    FILE *file;
    unsigned int line;

    if (path == NULL || pattern == NULL) {
        return 0;
    }

    file = fopen(path, "w");
    if (file == NULL) {
        return 0;
    }

    for (line = 1U; line <= 105U; ++line) {
        if (fprintf(file,
                    "%s line %u\n",
                    pattern,
                    line) < 0) {
            (void)fclose(file);
            return 0;
        }
    }

    if (fclose(file) != 0) {
        return 0;
    }

    return 1;
}

static int write_context_file(const char *path,
                              const char *pattern)
{
    FILE *file;

    if (path == NULL || pattern == NULL) {
        return 0;
    }

    file = fopen(path, "w");
    if (file == NULL) {
        return 0;
    }

    if (fprintf(file,
                "line 1\n"
                "line 2\n"
                "%s first match\n"
                "line 4\n"
                "%s second match\n"
                "line 6\n"
                "line 7\n"
                "line 8\n"
                "line 9\n"
                "line 10\n"
                "line 11\n"
                "line 12\n"
                "line 13\n"
                "line 14\n"
                "line 15\n"
                "line 16\n"
                "line 17\n"
                "line 18\n"
                "line 19\n"
                "line 20\n"
                "line 21\n"
                "line 22\n"
                "line 23\n"
                "line 24\n"
                "line 25\n",
                pattern,
                pattern) < 0) {
        (void)fclose(file);
        return 0;
    }

    if (fclose(file) != 0) {
        return 0;
    }

    return 1;
}

static char *read_output(const char *path)
{
    FILE *file;
    char *buffer;
    size_t used;
    size_t amount;

    if (path == NULL) {
        return NULL;
    }

    file = fopen(path, "r");
    if (file == NULL) {
        return NULL;
    }

    buffer = malloc(TEST_BUFFER_SIZE);
    if (buffer == NULL) {
        (void)fclose(file);
        return NULL;
    }

    used = 0U;

    while (used + 1U < TEST_BUFFER_SIZE) {
        amount = fread(buffer + used,
                       1U,
                       TEST_BUFFER_SIZE - used - 1U,
                       file);

        used += amount;

        if (amount == 0U) {
            break;
        }
    }

    buffer[used] = '\0';

    if (ferror(file)) {
        free(buffer);
        (void)fclose(file);
        return NULL;
    }

    (void)fclose(file);
    return buffer;
}

static int contains_ignore_case(const char *text,
                                const char *pattern)
{
    const unsigned char *start;

    if (text == NULL ||
        pattern == NULL ||
        *pattern == '\0') {
        return 0;
    }

    for (start = (const unsigned char *)text;
         *start != (unsigned char)'\0';
         ++start) {
        const unsigned char *left;
        const unsigned char *right;

        left = start;
        right = (const unsigned char *)pattern;

        while (*left != (unsigned char)'\0' &&
               *right != (unsigned char)'\0' &&
               tolower((int)*left) ==
                   tolower((int)*right)) {
            ++left;
            ++right;
        }

        if (*right == (unsigned char)'\0') {
            return 1;
        }
    }

    return 0;
}

static unsigned long count_ignore_case(const char *text,
                                       const char *pattern)
{
    const unsigned char *position;
    size_t pattern_length;
    unsigned long count;

    if (text == NULL ||
        pattern == NULL ||
        *pattern == '\0') {
        return 0UL;
    }

    pattern_length = strlen(pattern);
    count = 0UL;
    position = (const unsigned char *)text;

    while (*position != (unsigned char)'\0') {
        const unsigned char *left;
        const unsigned char *right;

        left = position;
        right = (const unsigned char *)pattern;

        while (*left != (unsigned char)'\0' &&
               *right != (unsigned char)'\0' &&
               tolower((int)*left) ==
                   tolower((int)*right)) {
            ++left;
            ++right;
        }

        if (*right == (unsigned char)'\0') {
            ++count;
            position += pattern_length;
        } else {
            ++position;
        }
    }

    return count;
}

static unsigned long count_text(const char *text,
                                const char *pattern)
{
    const char *position;
    size_t length;
    unsigned long count;

    if (text == NULL ||
        pattern == NULL ||
        *pattern == '\0') {
        return 0UL;
    }

    position = text;
    length = strlen(pattern);
    count = 0UL;

    while ((position = strstr(position, pattern)) != NULL) {
        ++count;
        position += length;
    }

    return count;
}

static int prepare_basic(const char *pattern)
{
    char source_text[256];
    char document_text[256];
    char test_text[256];
    char skipped_text[256];

    if (snprintf(source_text,
                 sizeof(source_text),
                 "first line\n%s source line\n",
                 pattern) < 0 ||
        snprintf(document_text,
                 sizeof(document_text),
                 "%s document line\n",
                 pattern) < 0 ||
        snprintf(test_text,
                 sizeof(test_text),
                 "%s test line\n",
                 pattern) < 0 ||
        snprintf(skipped_text,
                 sizeof(skipped_text),
                 "%s excluded line\n",
                 pattern) < 0) {
        return 0;
    }

    return write_text("SRC/M188_GREP_A.C",
                      source_text) &&
           write_text("DOC/M188_GREP_B.MD",
                      document_text) &&
           write_text("TEST/M188_GREP_C.TXT",
                      test_text) &&
           write_text("M188_GREP_SKIP.C",
                      skipped_text);
}

static int verify_basic(const char *output,
                        const char *pattern)
{
    if (output == NULL || pattern == NULL) {
        return 0;
    }

    if (!contains_ignore_case(
            output,
            "SRC/M188_GREP_A.C:2:") ||
        !contains_ignore_case(
            output,
            "DOC/M188_GREP_B.MD:1:") ||
        !contains_ignore_case(
            output,
            "TEST/M188_GREP_C.TXT:1:")) {
        return 0;
    }

    if (contains_ignore_case(output,
                             "M188_GREP_SKIP.C")) {
        return 0;
    }

    if (!contains_ignore_case(output,
                              "3 matches found.")) {
        return 0;
    }

    return count_text(output, pattern) == 3UL;
}

static int verify_limit(const char *output,
                        const char *pattern)
{
    if (output == NULL || pattern == NULL) {
        return 0;
    }

    if (!contains_ignore_case(
            output,
            "100 matches shown; result limit reached.")) {
        return 0;
    }

    if (count_text(output, pattern) != 100UL) {
        return 0;
    }

    if (contains_ignore_case(output,
                             "line 101")) {
        return 0;
    }

    return 1;
}

static int verify_targeted(const char *output,
                           const char *pattern)
{
    if (output == NULL || pattern == NULL) {
        return 0;
    }

    /*
     * Directory search should find only the DOC fixture.
     * The source fixture appears twice: once through a Unix-style
     * path and once through an OpenVMS-style relative path.
     */
    if (count_ignore_case(
            output,
            "DOC/M188_GREP_B.MD:1:") != 1UL ||
        count_ignore_case(
            output,
            "SRC/M188_GREP_A.C:2:") != 2UL) {
        return 0;
    }

    if (contains_ignore_case(
            output,
            "TEST/M188_GREP_C.TXT")) {
        return 0;
    }

    if (count_text(output,
                   pattern) != 3UL) {
        return 0;
    }

    if (count_text(output,
                   "1 match found.") != 3UL) {
        return 0;
    }

    if (!contains_ignore_case(
            output,
            "Unable to search M189_MISSING_PATH:")) {
        return 0;
    }

    if (!contains_ignore_case(
            output,
            "Unsafe or invalid project-relative path.")) {
        return 0;
    }

    return 1;
}

static int verify_context(const char *output,
                          const char *pattern)
{
    unsigned long line;

    if (output == NULL || pattern == NULL) {
        return 0;
    }

    if (count_text(output, pattern) != 2UL ||
        !contains_ignore_case(
            output,
            "TEST/M190_GREP_CONTEXT.TXT:3:") ||
        !contains_ignore_case(
            output,
            "TEST/M190_GREP_CONTEXT.TXT:5:") ||
        !contains_ignore_case(
            output,
            "TEST/M190_GREP_CONTEXT.TXT-1-") ||
        !contains_ignore_case(
            output,
            "TEST/M190_GREP_CONTEXT.TXT-7-") ||
        !contains_ignore_case(
            output,
            "2 matches found.")) {
        return 0;
    }

    /*
     * Overlapping ranges must print each source line only once.
     */
    for (line = 1UL; line <= 7UL; ++line) {
        char marker_colon[96];
        char marker_hyphen[96];
        unsigned long occurrences;

        (void)snprintf(
            marker_colon,
            sizeof(marker_colon),
            "M190_GREP_CONTEXT.TXT:%lu:",
            line
        );

        (void)snprintf(
            marker_hyphen,
            sizeof(marker_hyphen),
            "M190_GREP_CONTEXT.TXT-%lu-",
            line
        );

        occurrences =
            count_ignore_case(output, marker_colon) +
            count_ignore_case(output, marker_hyphen);

        if (occurrences != 1UL) {
            return 0;
        }
    }

    return 1;
}

static int verify_zero_context(const char *output,
                               const char *pattern)
{
    if (output == NULL || pattern == NULL) {
        return 0;
    }

    if (count_text(output, pattern) != 2UL ||
        count_ignore_case(
            output,
            "M190_GREP_CONTEXT.TXT:3:") != 1UL ||
        count_ignore_case(
            output,
            "M190_GREP_CONTEXT.TXT:5:") != 1UL ||
        contains_ignore_case(
            output,
            "M190_GREP_CONTEXT.TXT-")) {
        return 0;
    }

    return contains_ignore_case(output,
                                "2 matches found.");
}

static int verify_max_context(const char *output,
                              const char *pattern)
{
    if (output == NULL || pattern == NULL) {
        return 0;
    }

    if (count_text(output, pattern) != 2UL ||
        !contains_ignore_case(
            output,
            "M190_GREP_CONTEXT.TXT-1-") ||
        !contains_ignore_case(
            output,
            "M190_GREP_CONTEXT.TXT-25-") ||
        !contains_ignore_case(
            output,
            "2 matches found.")) {
        return 0;
    }

    return 1;
}

int main(void)
{
    agent_state state;
    char basic_pattern[64];
    char limit_pattern[64];
    char *basic_output;
    char *limit_output;
    char *path_output;
    char *context_output;
    char *zero_output;
    char *max_output;
    int result;

    cleanup_files();

    (void)strcpy(basic_pattern, "M188_");
    (void)strcat(basic_pattern, "BASIC_TOKEN");

    (void)strcpy(limit_pattern, "M188_");
    (void)strcat(limit_pattern, "LIMIT_TOKEN");

    if (!prepare_basic(basic_pattern) ||
        !write_limit_file("TEST/M188_GREP_LIMIT.TXT",
                          limit_pattern)) {
        cleanup_files();
        (void)fprintf(stderr,
                      "GREP test fixture creation failed.\n");
        return EXIT_FAILURE;
    }

    state.running = 1;
    state.project_root = ".";
    state.api_key_defined = 0;
    state.write_enabled = 0;
    state.dcl_enabled = 0;

    if (freopen("M188_GREP_BASIC.OUT",
                "w",
                stdout) == NULL) {
        cleanup_files();
        (void)fprintf(stderr,
                      "Unable to capture basic GREP output.\n");
        return EXIT_FAILURE;
    }

    project_grep(&state, basic_pattern, NULL, 0U);
    (void)fflush(stdout);

    if (freopen("M188_GREP_LIMIT.OUT",
                "w",
                stdout) == NULL) {
        cleanup_files();
        (void)fprintf(stderr,
                      "Unable to capture limit GREP output.\n");
        return EXIT_FAILURE;
    }

    project_grep(&state, limit_pattern, NULL, 0U);
    (void)fflush(stdout);

    if (!write_context_file(
            "TEST/M190_GREP_CONTEXT.TXT",
            basic_pattern)) {
        cleanup_files();
        (void)fprintf(stderr,
                      "GREP context fixture creation failed.\n");
        return EXIT_FAILURE;
    }

    if (freopen("M189_GREP_PATH.OUT",
                "w",
                stdout) == NULL) {
        cleanup_files();
        (void)fprintf(stderr,
                      "Unable to capture path GREP output.\n");
        return EXIT_FAILURE;
    }

    project_grep(&state,
                 basic_pattern,
                 "DOC",
                  0U);

    project_grep(&state,
                 basic_pattern,
                 "SRC/M188_GREP_A.C", 0U);

    project_grep(&state,
                 basic_pattern,
                 "[.SRC]M188_GREP_A.C", 0U);

    project_grep(&state,
                 basic_pattern,
                 "M189_MISSING_PATH", 0U);

    project_grep(&state,
                 basic_pattern,
                 "../SRC",
                 0U);

    (void)fflush(stdout);

    if (freopen("M190_GREP_CONTEXT.OUT",
                "w",
                stdout) == NULL) {
        cleanup_files();
        (void)fprintf(stderr,
                      "Unable to capture GREP context output.\n");
        return EXIT_FAILURE;
    }

    project_grep(&state,
                 basic_pattern,
                 "TEST/M190_GREP_CONTEXT.TXT",
                 2U);

    (void)fflush(stdout);

    if (freopen("M190_GREP_ZERO.OUT",
                "w",
                stdout) == NULL) {
        cleanup_files();
        (void)fprintf(stderr,
                      "Unable to capture zero-context output.\n");
        return EXIT_FAILURE;
    }

    project_grep(&state,
                 basic_pattern,
                 "TEST/M190_GREP_CONTEXT.TXT",
                 0U);

    (void)fflush(stdout);

    if (freopen("M190_GREP_MAX.OUT",
                "w",
                stdout) == NULL) {
        cleanup_files();
        (void)fprintf(stderr,
                      "Unable to capture maximum-context output.\n");
        return EXIT_FAILURE;
    }

    project_grep(&state,
                 basic_pattern,
                 "TEST/M190_GREP_CONTEXT.TXT",
                 20U);

    (void)fflush(stdout);
    (void)fclose(stdout);

    basic_output = read_output("M188_GREP_BASIC.OUT");
    limit_output = read_output("M188_GREP_LIMIT.OUT");
    path_output = read_output("M189_GREP_PATH.OUT");
    context_output = read_output("M190_GREP_CONTEXT.OUT");
    zero_output = read_output("M190_GREP_ZERO.OUT");
    max_output = read_output("M190_GREP_MAX.OUT");

    result = verify_basic(basic_output,
                          basic_pattern) &&
             verify_limit(limit_output,
                          limit_pattern) &&
             verify_targeted(path_output,
                             basic_pattern) &&
             verify_context(context_output,
                            basic_pattern) &&
             verify_zero_context(zero_output,
                                 basic_pattern) &&
             verify_max_context(max_output,
                                basic_pattern);

    free(basic_output);
    free(limit_output);
    free(path_output);
    free(context_output);
    free(zero_output);
    free(max_output);
    cleanup_files();

    if (!result) {
        (void)fprintf(stderr,
                      "Repository GREP regression test failed.\n");
        return EXIT_FAILURE;
    }

    (void)fprintf(stderr,
                  "Repository GREP regression test passed.\n");
    return EXIT_SUCCESS;
}
