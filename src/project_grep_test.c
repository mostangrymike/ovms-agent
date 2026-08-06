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
    remove_all("TEST/M194_GREP_CASE.TXT");
    remove_all("M194_GREP_SENSITIVE.OUT");
    remove_all("M194_GREP_INSENSITIVE.OUT");
    remove_all("M196_GREP_COUNT.OUT");
    remove_all("SRC/M197_FILES_A.C");
    remove_all("DOC/M197_FILES_B.MD");
    remove_all("TEST/M197_FILES_C.TXT");
    remove_all("M197_GREP_FILES.OUT");
    remove_all("M197_GREP_FILES_LIMIT.OUT");
    remove_all("M198_GREP_NAME.OUT");
    remove_all("M198_GREP_NAME_FILES.OUT");
    remove_all("M199_GREP_EXCLUDE.OUT");
    remove_all("M200_DEPTH_ROOT.C");
    remove_all("SRC/M200_DEPTH_CHILD.C");
    remove_all("M200_GREP_DEPTH_ZERO.OUT");
    remove_all("M200_GREP_DEPTH_ONE.OUT");
    remove_all("TEST/M201_GREP_WORD.TXT");
    remove_all("M201_GREP_WORD.OUT");
    remove_all("M201_GREP_WORD_CASE.OUT");
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

static int write_case_file(const char *path)
{
    return write_text(
        path,
        "CaseMarker exact case\n"
        "casemarker lowercase\n"
        "CASEMARKER uppercase\n"
        "unrelated line\n"
    );
}

static int prepare_files_only(const char *pattern)
{
    char source_text[256];
    char document_text[256];
    char test_text[256];

    if (pattern == NULL) {
        return 0;
    }

    if (snprintf(source_text,
                 sizeof(source_text),
                 "%s first source match\n"
                 "%s second source match\n",
                 pattern,
                 pattern) < 0 ||
        snprintf(document_text,
                 sizeof(document_text),
                 "%s document match\n",
                 pattern) < 0 ||
        snprintf(test_text,
                 sizeof(test_text),
                 "%s test match\n",
                 pattern) < 0) {
        return 0;
    }

    return write_text("SRC/M197_FILES_A.C",
                      source_text) &&
           write_text("DOC/M197_FILES_B.MD",
                      document_text) &&
           write_text("TEST/M197_FILES_C.TXT",
                      test_text);
}

static int prepare_depth(const char *pattern)
{
    char root_text[128];
    char child_text[128];

    if (pattern == NULL ||
        snprintf(root_text,
                 sizeof(root_text),
                 "%s root\n",
                 pattern) < 0 ||
        snprintf(child_text,
                 sizeof(child_text),
                 "%s child\n",
                 pattern) < 0) {
        return 0;
    }

    return write_text("M200_DEPTH_ROOT.C",
                      root_text) &&
           write_text("SRC/M200_DEPTH_CHILD.C",
                      child_text);
}

static int write_word_file(const char *path)
{
    return write_text(
        path,
        "project standalone\n"
        "project_grep identifier\n"
        "projects plural\n"
        "myproject prefixed\n"
        "(project) punctuation\n"
        "PROJECT uppercase\n"
    );
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

static int verify_case_matching(
    const char *sensitive_output,
    const char *insensitive_output)
{
    if (sensitive_output == NULL ||
        insensitive_output == NULL) {
        return 0;
    }

    /*
     * Sensitive search must match only the exact-case line.
     */
    if (count_text(sensitive_output,
                   "CaseMarker") != 1UL ||
        strstr(sensitive_output,
               "casemarker lowercase") != NULL ||
        strstr(sensitive_output,
               "CASEMARKER uppercase") != NULL ||
        !contains_ignore_case(
            sensitive_output,
            "1 match found.")) {
        return 0;
    }

    /*
     * Insensitive search must match all three case variants.
     */
    if (count_ignore_case(
            insensitive_output,
            "casemarker") != 3UL ||
        !contains_ignore_case(
            insensitive_output,
            "M194_GREP_CASE.TXT:1:") ||
        !contains_ignore_case(
            insensitive_output,
            "M194_GREP_CASE.TXT:2:") ||
        !contains_ignore_case(
            insensitive_output,
            "M194_GREP_CASE.TXT:3:") ||
        !contains_ignore_case(
            insensitive_output,
            "3 matches found.")) {
        return 0;
    }

    return 1;
}

static int verify_count_output(const char *output)
{
    return output != NULL &&
           strstr(output, "2 matches found.") != NULL &&
           strstr(output, "TEST/M190_GREP_CONTEXT.TXT:") == NULL &&
           strstr(output, "first match") == NULL &&
           strstr(output, "second match") == NULL;
}

static int verify_files_output(const char *output,
                               const char *limit_output,
                               const char *pattern)
{
    if (output == NULL ||
        limit_output == NULL ||
        pattern == NULL) {
        return 0;
    }

    if (count_ignore_case(output,
                          "SRC/M197_FILES_A.C") != 1UL ||
        count_ignore_case(output,
                          "DOC/M197_FILES_B.MD") != 1UL ||
        count_ignore_case(output,
                          "TEST/M197_FILES_C.TXT") != 1UL ||
        !contains_ignore_case(output,
                              "3 matching files found.") ||
        contains_ignore_case(output, pattern)) {
        return 0;
    }

    if (count_ignore_case(limit_output,
                          "SRC/M197_FILES_A.C") != 1UL ||
        count_ignore_case(limit_output,
                          "DOC/M197_FILES_B.MD") != 1UL ||
        contains_ignore_case(limit_output,
                             "TEST/M197_FILES_C.TXT") ||
        !contains_ignore_case(
            limit_output,
            "2 matching files shown; result limit reached.") ||
        contains_ignore_case(limit_output, pattern)) {
        return 0;
    }

    return 1;
}

static int verify_name_output(const char *output,
                              const char *files_output,
                              const char *pattern)
{
    if (output == NULL ||
        files_output == NULL ||
        pattern == NULL) {
        return 0;
    }

    if (count_ignore_case(output,
                          "SRC/M197_FILES_A.C") != 2UL ||
        contains_ignore_case(output,
                             "DOC/M197_FILES_B.MD") ||
        contains_ignore_case(output,
                             "TEST/M197_FILES_C.TXT") ||
        count_text(output, pattern) != 2UL ||
        !contains_ignore_case(output,
                              "2 matches found.")) {
        return 0;
    }

    return count_ignore_case(files_output,
                             "DOC/M197_FILES_B.MD") == 1UL &&
           !contains_ignore_case(files_output,
                                 "SRC/M197_FILES_A.C") &&
           !contains_ignore_case(files_output,
                                 "TEST/M197_FILES_C.TXT") &&
           !contains_ignore_case(files_output, pattern) &&
           contains_ignore_case(files_output,
                                "1 matching file found.");
}

static int verify_exclude_output(const char *output,
                                 const char *pattern)
{
    if (output == NULL || pattern == NULL) {
        return 0;
    }

    return contains_ignore_case(output,
                                "SRC/M188_GREP_A.C") &&
           contains_ignore_case(output,
                                "DOC/M188_GREP_B.MD") &&
           contains_ignore_case(output,
                                "TEST/M188_GREP_C.TXT") &&
           !contains_ignore_case(output,
                                 "M190_GREP_CONTEXT.TXT") &&
           count_text(output, pattern) == 3UL &&
           contains_ignore_case(output,
                                "3 matches found.");
}

static int verify_depth_output(const char *zero_output,
                               const char *one_output,
                               const char *pattern)
{
    if (zero_output == NULL ||
        one_output == NULL ||
        pattern == NULL) {
        return 0;
    }

    if (!contains_ignore_case(zero_output, "M200_DEPTH_ROOT.C") ||
        contains_ignore_case(zero_output, "M200_DEPTH_CHILD.C") ||
        !contains_ignore_case(zero_output, "1 match found.")) {
        return 0;
    }

    return contains_ignore_case(one_output, "M200_DEPTH_ROOT.C") &&
           contains_ignore_case(one_output, "M200_DEPTH_CHILD.C") &&
           contains_ignore_case(one_output, "2 matches found.") &&
           count_text(one_output, pattern) == 2UL;
}

static int verify_word_output(
    const char *insensitive_output,
    const char *sensitive_output)
{
    if (insensitive_output == NULL ||
        sensitive_output == NULL) {
        return 0;
    }

    if (strstr(insensitive_output,
               "M201_GREP_WORD.TXT:1:") == NULL ||
        strstr(insensitive_output,
               "M201_GREP_WORD.TXT:5:") == NULL ||
        strstr(insensitive_output,
               "M201_GREP_WORD.TXT:6:") == NULL ||
        strstr(insensitive_output,
               "project_grep identifier") != NULL ||
        strstr(insensitive_output,
               "projects plural") != NULL ||
        strstr(insensitive_output,
               "myproject prefixed") != NULL ||
        strstr(insensitive_output,
               "3 matches found.") == NULL) {
        return 0;
    }

    return strstr(sensitive_output,
                  "M201_GREP_WORD.TXT:1:") != NULL &&
           strstr(sensitive_output,
                  "M201_GREP_WORD.TXT:5:") != NULL &&
           strstr(sensitive_output,
                  "M201_GREP_WORD.TXT:6:") == NULL &&
           strstr(sensitive_output,
                  "2 matches found.") != NULL;
}

int main(void)
{
    agent_state state;
    char basic_pattern[64];
    char limit_pattern[64];
    char files_pattern[64];
    char *basic_output;
    char *limit_output;
    char *path_output;
    char *context_output;
    char *zero_output;
    char *max_output;
    char *sensitive_output;
    char *insensitive_output;
    char *count_output;
    char *files_output;
    char *files_limit_output;
    char *name_output;
    char *name_files_output;
    char *exclude_output;
    char *depth_zero_output;
    char *depth_one_output;
    char *word_output;
    char *word_case_output;
    char depth_pattern[64];
    int result;

    cleanup_files();

    (void)strcpy(basic_pattern, "M188_");
    (void)strcat(basic_pattern, "BASIC_TOKEN");

    (void)strcpy(limit_pattern, "M188_");
    (void)strcat(limit_pattern, "LIMIT_TOKEN");

    (void)strcpy(files_pattern, "M197_");
    (void)strcat(files_pattern, "FILES_TOKEN");

    (void)strcpy(depth_pattern, "M200_");
    (void)strcat(depth_pattern, "DEPTH_TOKEN");

    if (!prepare_basic(basic_pattern) ||
        !write_limit_file("TEST/M188_GREP_LIMIT.TXT",
                          limit_pattern) ||
        !write_case_file("TEST/M194_GREP_CASE.TXT") ||
        !prepare_files_only(files_pattern) ||
        !prepare_depth(depth_pattern)) {
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

    project_grep(&state, basic_pattern, NULL, 0U, 100UL, 0, 0, 0,
    NULL,
                  NULL,
                  16U,
            0);
    (void)fflush(stdout);

    if (freopen("M188_GREP_LIMIT.OUT",
                "w",
                stdout) == NULL) {
        cleanup_files();
        (void)fprintf(stderr,
                      "Unable to capture limit GREP output.\n");
        return EXIT_FAILURE;
    }

    project_grep(&state, limit_pattern, NULL, 0U, 100UL, 0, 0, 0,
    NULL,
                  NULL,
                  16U,
            0);
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
                  0U, 100UL, 0, 0, 0,
                      NULL,
                  NULL,
                  16U,
            0);

    project_grep(&state,
                 basic_pattern,
                 "SRC/M188_GREP_A.C", 0U, 100UL, 0, 0, 0,
                     NULL,
                  NULL,
                  16U,
            0);

    project_grep(&state,
                 basic_pattern,
                 "[.SRC]M188_GREP_A.C", 0U, 100UL, 0, 0, 0,
                     NULL,
                  NULL,
                  16U,
            0);

    project_grep(&state,
                 basic_pattern,
                 "M189_MISSING_PATH", 0U, 100UL, 0, 0, 0,
                     NULL,
                  NULL,
                  16U,
            0);

    project_grep(&state,
                 basic_pattern,
                 "../SRC",
                 0U, 100UL, 0, 0, 0,
                     NULL,
                  NULL,
                  16U,
            0);

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
                 2U, 100UL, 0, 0, 0,
                     NULL,
                  NULL,
                  16U,
            0);
    (void)fflush(stdout);
    if (freopen("M194_GREP_SENSITIVE.OUT",
                "w",
                stdout) == NULL) {
        cleanup_files();
        (void)fprintf(
            stderr,
            "Unable to capture case-sensitive output.\n"
        );
        return EXIT_FAILURE;
    }

    project_grep(&state,
                 "CaseMarker",
                 "TEST/M194_GREP_CASE.TXT",
                 0U,
                 100UL,
                 1,
                 0, 0,
                     NULL,
                  NULL,
                  16U,
            0);
    (void)fflush(stdout);

    if (freopen("M194_GREP_INSENSITIVE.OUT",
                "w",
                stdout) == NULL) {
        cleanup_files();
        (void)fprintf(
            stderr,
            "Unable to capture case-insensitive output.\n"
        );
        return EXIT_FAILURE;
    }

    project_grep(&state,
                 "CaseMarker",
                 "TEST/M194_GREP_CASE.TXT",
                 0U,
                 100UL,
                 0,
                 0, 0,
                     NULL,
                  NULL,
                  16U,
            0);

    (void)fflush(stdout);

    if (freopen("M196_GREP_COUNT.OUT",
                "w",
                stdout) == NULL) {
        cleanup_files();
        (void)fprintf(stderr,
                      "Unable to capture count-only output.\n");
        return EXIT_FAILURE;
    }

    project_grep(&state,
                 basic_pattern,
                 "TEST/M190_GREP_CONTEXT.TXT",
                 0U, 100UL, 0, 1, 0,
                     NULL,
                  NULL,
                  16U,
            0);

    (void)fflush(stdout);

    if (freopen("M197_GREP_FILES.OUT",
                "w",
                stdout) == NULL) {
        cleanup_files();
        (void)fprintf(stderr,
                      "Unable to capture files-only output.\n");
        return EXIT_FAILURE;
    }

    project_grep(&state,
                 files_pattern,
                 NULL,
                 0U, 100UL, 0, 0, 1,
                     NULL,
                  NULL,
                  16U,
            0);

    (void)fflush(stdout);

    if (freopen("M197_GREP_FILES_LIMIT.OUT",
                "w",
                stdout) == NULL) {
        cleanup_files();
        (void)fprintf(
            stderr,
            "Unable to capture limited files-only output.\n"
        );
        return EXIT_FAILURE;
    }

    project_grep(&state,
                 files_pattern,
                 NULL,
                 0U, 2UL, 0, 0, 1,
                     NULL,
                  NULL,
                  16U,
            0);

    (void)fflush(stdout);

    if (freopen("M198_GREP_NAME.OUT",
                "w",
                stdout) == NULL) {
        cleanup_files();
        (void)fprintf(stderr,
                      "Unable to capture name-filter output.\n");
        return EXIT_FAILURE;
    }

    project_grep(&state,
                 files_pattern,
                 NULL,
                 0U, 100UL, 0, 0, 0,
                 "*.C",
                  NULL,
                  16U,
            0);

    (void)fflush(stdout);

    if (freopen("M198_GREP_NAME_FILES.OUT",
                "w",
                stdout) == NULL) {
        cleanup_files();
        (void)fprintf(
            stderr,
            "Unable to capture filtered files-only output.\n"
        );
        return EXIT_FAILURE;
    }

    project_grep(&state,
                 files_pattern,
                 NULL,
                 0U, 100UL, 0, 0, 1,
                 "*.MD",
                  NULL,
                  16U,
            0);

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
                 0U, 100UL, 0, 0, 0,
                     NULL,
                  NULL,
                  16U,
            0);

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
                 20U, 100UL, 0, 0, 0,
                     NULL,
                  NULL,
                  16U,
            0);

    (void)fflush(stdout);

    if (freopen("M199_GREP_EXCLUDE.OUT",
                "w",
                stdout) == NULL) {
        cleanup_files();
        (void)fprintf(stderr,
                      "Unable to capture exclude output.\n");
        return EXIT_FAILURE;
    }

    project_grep(&state,
                 basic_pattern,
                 NULL,
                 0U, 100UL, 0, 0, 0,
                 NULL,
                 "M19*",
                  16U,
            0);

    (void)fflush(stdout);

    if (freopen("M200_GREP_DEPTH_ZERO.OUT",
                "w",
                stdout) == NULL) {
        cleanup_files();
        (void)fprintf(stderr,
                      "Unable to capture depth-zero output.\n");
        return EXIT_FAILURE;
    }

    project_grep(&state, depth_pattern, ".", 0U, 100UL, 0, 0, 0,
                 NULL, NULL, 0U,
            0);
    (void)fflush(stdout);

    if (freopen("M200_GREP_DEPTH_ONE.OUT",
                "w",
                stdout) == NULL) {
        cleanup_files();
        (void)fprintf(stderr,
                      "Unable to capture depth-one output.\n");
        return EXIT_FAILURE;
    }

    project_grep(&state, depth_pattern, ".", 0U, 100UL, 0, 0, 0,
                 NULL, NULL, 1U,
            0);
    (void)fflush(stdout);

    if (freopen("M201_GREP_WORD.OUT",
                "w",
                stdout) == NULL) {
        cleanup_files();
        (void)fprintf(stderr,
                      "Unable to capture word output.\n");
        return EXIT_FAILURE;
    }

    project_grep(&state,
                 "project",
                 "TEST/M201_GREP_WORD.TXT",
                 0U,
                 100UL,
                 0,
                 0,
                 0,
                 NULL,
                 NULL,
                 16U,
                 1);
    (void)fflush(stdout);

    if (freopen("M201_GREP_WORD_CASE.OUT",
                "w",
                stdout) == NULL) {
        cleanup_files();
        (void)fprintf(stderr,
                      "Unable to capture word case output.\n");
        return EXIT_FAILURE;
    }

    project_grep(&state,
                 "project",
                 "TEST/M201_GREP_WORD.TXT",
                 0U,
                 100UL,
                 1,
                 0,
                 0,
                 NULL,
                 NULL,
                 16U,
                 1);
    (void)fflush(stdout);
    (void)fclose(stdout);

    basic_output = read_output("M188_GREP_BASIC.OUT");
    limit_output = read_output("M188_GREP_LIMIT.OUT");
    path_output = read_output("M189_GREP_PATH.OUT");
    context_output = read_output("M190_GREP_CONTEXT.OUT");
    zero_output = read_output("M190_GREP_ZERO.OUT");
    max_output = read_output("M190_GREP_MAX.OUT");
    sensitive_output =
        read_output("M194_GREP_SENSITIVE.OUT");
    insensitive_output =
        read_output("M194_GREP_INSENSITIVE.OUT");
    count_output = read_output("M196_GREP_COUNT.OUT");
    files_output = read_output("M197_GREP_FILES.OUT");
    files_limit_output =
        read_output("M197_GREP_FILES_LIMIT.OUT");
    name_output = read_output("M198_GREP_NAME.OUT");
    name_files_output =
        read_output("M198_GREP_NAME_FILES.OUT");
    exclude_output =
        read_output("M199_GREP_EXCLUDE.OUT");
    depth_zero_output =
        read_output("M200_GREP_DEPTH_ZERO.OUT");
    depth_one_output =
        read_output("M200_GREP_DEPTH_ONE.OUT");
    word_output = read_output("M201_GREP_WORD.OUT");
    word_case_output =
        read_output("M201_GREP_WORD_CASE.OUT");

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
                                  basic_pattern) &&
               verify_case_matching(
                   sensitive_output,
                   insensitive_output
               ) &&
               verify_count_output(count_output) &&
               verify_files_output(files_output,
                                   files_limit_output,
                                   files_pattern) &&
               verify_name_output(name_output,
                                  name_files_output,
                                  files_pattern) &&
               verify_exclude_output(exclude_output,
                                     basic_pattern) &&
               verify_depth_output(depth_zero_output,
                                   depth_one_output,
                                   depth_pattern);

    free(basic_output);
    free(limit_output);
    free(path_output);
    free(context_output);
    free(zero_output);
    free(max_output);
    free(sensitive_output);
    free(insensitive_output);
    free(count_output);
    free(files_output);
    free(files_limit_output);
    free(name_output);
    free(name_files_output);
    free(exclude_output);
    free(depth_zero_output);
    free(depth_one_output);
    free(word_output);
    free(word_case_output);
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
