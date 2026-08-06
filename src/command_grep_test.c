#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "agent.h"
#include "command_internal.h"
#include "edit.h"
#include "project.h"

#define TEST_OUTPUT "M192_COMMAND_GREP.OUT"
#define TEST_PATH_SIZE 256
#define TEST_PATTERN_SIZE 256

static unsigned int grep_calls;
static char recorded_pattern[TEST_PATTERN_SIZE];
static char recorded_path[TEST_PATH_SIZE];
static unsigned int recorded_context;
static unsigned long recorded_limit;
static int recorded_case_sensitive;
static int recorded_count_only;

static void remove_all(const char *path)
{
    char versioned[TEST_PATH_SIZE];

    if (path == NULL) {
        return;
    }

    (void)remove(path);

    if (snprintf(versioned,
                 sizeof(versioned),
                 "%s;*", path) >= 0) {
        while (remove(versioned) == 0) {
            /* Remove all OpenVMS file versions. */
        }
    }
}

static char *read_output(const char *path)
{
    FILE *file;
    char *text;
    long length;
    size_t count;

    file = fopen(path, "r");
    if (file == NULL) {
        return NULL;
    }

    if (fseek(file, 0L, SEEK_END) != 0) {
        (void)fclose(file);
        return NULL;
    }

    length = ftell(file);
    if (length < 0L) {
        (void)fclose(file);
        return NULL;
    }

    if (fseek(file, 0L, SEEK_SET) != 0) {
        (void)fclose(file);
        return NULL;
    }

    text = (char *)malloc((size_t)length + 1U);
    if (text == NULL) {
        (void)fclose(file);
        return NULL;
    }

    count = fread(text, 1U, (size_t)length, file);
    text[count] = '\0';

    if (fclose(file) != 0) {
        free(text);
        return NULL;
    }

    return text;
}

static int contains_text(const char *text,
                         const char *pattern)
{
    return text != NULL &&
           pattern != NULL &&
           strstr(text, pattern) != NULL;
}

static void reset_recording(void)
{
    grep_calls = 0U;
    recorded_pattern[0] = '\0';
    recorded_path[0] = '\0';
    recorded_context = 0U;
    recorded_limit = 0UL;
    recorded_case_sensitive = 0;
    recorded_count_only = 0;
}

static int run_command(agent_state *state,
                       const char *arguments,
                       char **output)
{
    FILE *capture;

    if (state == NULL ||
        arguments == NULL ||
        output == NULL) {
        return 0;
    }

    *output = NULL;
    remove_all(TEST_OUTPUT);

    capture = freopen(TEST_OUTPUT, "w", stdout);
    if (capture == NULL) {
        return 0;
    }

    command_grep(state, arguments);

    (void)fflush(stdout);
    (void)fclose(stdout);

    *output = read_output(TEST_OUTPUT);
    return *output != NULL;
}

static int verify_call(const char *pattern,
                       const char *path,
                       unsigned int context,
                       unsigned long limit,
                       int case_sensitive,
                       int count_only)
{
    if (grep_calls != 1U ||
        strcmp(recorded_pattern, pattern) != 0 ||
        recorded_context != context ||
        recorded_limit != limit ||
        recorded_case_sensitive != case_sensitive ||
        recorded_count_only != count_only) {
        return 0;
    }

    if (path == NULL) {
        return recorded_path[0] == '\0';
    }

    return strcmp(recorded_path, path) == 0;
}

static int test_valid(agent_state *state,
                      const char *arguments,
                      const char *pattern,
                      const char *path,
                      unsigned int context,
                      unsigned long limit,
                      int case_sensitive,
                      int count_only)

{
    char *output;
    int result;

    reset_recording();

    if (!run_command(state, arguments, &output)) {
        return 0;
    }

    result = verify_call(pattern,
                         path,
                         context,
                         limit,
                         case_sensitive,
                         count_only);

    free(output);
    return result;
}

static int test_invalid(agent_state *state,
                        const char *arguments,
                        const char *diagnostic)
{
    char *output;
    int result;

    reset_recording();

    if (!run_command(state, arguments, &output)) {
        return 0;
    }

    result = grep_calls == 0U &&
             contains_text(output, diagnostic);

    free(output);
    return result;
}

/*
 * Production-call recorder.
 */
void project_grep(const agent_state *state,
                  const char *pattern,
                  const char *path,
                  unsigned int context,
                  unsigned long match_limit,
                  int case_sensitive,
                  int count_only)
{
    (void)state;

    ++grep_calls;

    if (pattern != NULL) {
        (void)strncpy(recorded_pattern,
                      pattern,
                      sizeof(recorded_pattern) - 1U);
        recorded_pattern[
            sizeof(recorded_pattern) - 1U
        ] = '\0';
    }

    if (path != NULL) {
        (void)strncpy(recorded_path,
                      path,
                      sizeof(recorded_path) - 1U);
        recorded_path[
            sizeof(recorded_path) - 1U
        ] = '\0';
    }

    recorded_context = context;
    recorded_limit = match_limit;
    recorded_case_sensitive = case_sensitive;
    recorded_count_only = count_only;
}

/*
 * Link-only stubs required by COMMAND_PROJECT.OBJ.
 */
const command_entry *command_find(const char *name)
{
    (void)name;
    return NULL;
}
int command_registry_add(const command_entry *entries,
                         size_t entry_count)
{
    (void)entries;
    (void)entry_count;
    return 1;
}

void edit_run(agent_state *state,
              const char *path)
{
    (void)state;
    (void)path;
}

void project_show_root(const agent_state *state)
{
    (void)state;
}

void project_show_status(const agent_state *state)
{
    (void)state;
}

void project_list(const agent_state *state,
                  const char *path,
                  int show_all)
{
    (void)state;
    (void)path;
    (void)show_all;
}

void project_read(const agent_state *state,
                  const char *path,
                  unsigned long start_line,
                  unsigned long line_count)
{
    (void)state;
    (void)path;
    (void)start_line;
    (void)line_count;
}

void project_git_status(const agent_state *state)
{
    (void)state;
}

void project_git_diff(const agent_state *state)
{
    (void)state;
}

void project_build(const agent_state *state)
{
    (void)state;
}

void project_search(const agent_state *state,
                    const char *path,
                    const char *pattern)
{
    (void)state;
    (void)path;
    (void)pattern;
}

void project_tree(const agent_state *state,
                  const char *path)
{
    (void)state;
    (void)path;
}

int project_patch(const agent_state *state,
                  const char *path,
                  const char *old_text,
                  const char *new_text)
{
    (void)state;
    (void)path;
    (void)old_text;
    (void)new_text;
    return 1;
}

int main(void)
{
    agent_state state;
    int result;

    state.running = 1;
    state.project_root = ".";
    state.api_key_defined = 0;
    state.write_enabled = 0;
    state.dcl_enabled = 0;

    result =
        test_valid(&state,
                   "\"token\" /LIMIT=3",
                   "token",
                   NULL,
                   0U,
                   3UL, 0, 0) &&
        test_valid(&state,
                   "\"token\" SRC /LIMIT=3",
                   "token",
                   "SRC",
                   0U,
                   3UL, 0, 0) &&
        test_valid(&state,
                   "\"token\" SRC /CONTEXT=2 /LIMIT=3",
                   "token",
                   "SRC",
                   2U,
                   3UL, 0, 0) &&
        test_valid(&state,
                   "\"token\" SRC /LIMIT=3 /CONTEXT=2",
                   "token",
                   "SRC",
                   2U,
                   3UL, 0, 0) &&
        test_valid(&state,
                   "\"token\" SRC",
                   "token",
                   "SRC",
                   0U,
                   100UL, 0, 0) &&
        test_valid(
            &state,
            "\"Token\" SRC /CASE=SENSITIVE",
            "Token",
            "SRC",
            0U,
            100UL,
            1,
            0
        ) &&
        test_valid(
            &state,
            "\"Token\" SRC /CASE=INSENSITIVE",
            "Token",
            "SRC",
            0U,
            100UL,
            0,
            0
        ) &&
        test_valid(
            &state,
            "\"Token\" SRC /LIMIT=3 "
            "/CASE=SENSITIVE /CONTEXT=2",
            "Token",
            "SRC",
            2U,
            3UL,
            1,
            0
        ) &&
        test_valid(
            &state,
            "\"token\" SRC /COUNT",
            "token",
            "SRC",
            0U,
            100UL,
            0,
            1
        ) &&
        test_valid(
            &state,
            "\"Token\" SRC /COUNT /CASE=SENSITIVE",
            "Token",
            "SRC",
            0U,
            100UL,
            1,
            1
        ) &&
        test_valid(
            &state,
            "\"token\" SRC /LIMIT=3 /COUNT",
            "token",
            "SRC",
            0U,
            3UL,
            0,
            1
        ) &&
        test_invalid(&state,
                     "\"token\" /LIMIT=0",
                     "Limit must be an integer from 1 to 100.") &&
        test_invalid(&state,
                     "\"token\" /LIMIT=101",
                     "Limit must be an integer from 1 to 100.") &&
        test_invalid(&state,
                     "\"token\" /LIMIT=X",
                     "Limit must be an integer from 1 to 100.") &&
        test_invalid(&state,
                     "\"token\" /LIMIT=3 /LIMIT=4",
                     "Duplicate /LIMIT qualifier.") &&
        test_invalid(&state,
                     "\"token\" /CONTEXT=1 /CONTEXT=2",
                     "Duplicate /CONTEXT qualifier.") &&
        test_invalid(&state,
                     "\"token\" /LIMIT=3 SRC",
                     "GREP path must precede qualifiers.") &&
        test_invalid(
            &state,
            "\"token\" /CASE=INVALID",
            "Case must be SENSITIVE or INSENSITIVE."
        ) &&
        test_invalid(
            &state,
            "\"token\" /CASE=SENSITIVE "
            "/CASE=INSENSITIVE",
            "Duplicate /CASE qualifier."
        ) &&
        test_invalid(
            &state,
            "\"token\" SRC /COUNT /COUNT",
            "Duplicate /COUNT qualifier."
        ) &&
        test_invalid(
            &state,
            "\"token\" SRC /COUNT=YES",
            "/COUNT does not take a value."
        ) &&
        test_invalid(
            &state,
            "\"token\" /COUNT SRC",
            "GREP path must precede qualifiers."
        ) &&
        test_invalid(
            &state,
            "\"token\" SRC /COUNT /CONTEXT=1",
            "/COUNT is not compatible with /CONTEXT."
        ) &&
        test_invalid(
            &state,
            "\"token\" SRC /CONTEXT=1 /COUNT",
            "/COUNT is not compatible with /CONTEXT."
        ) &&
        test_invalid(
            &state,
            "\"token\" SRC /UNKNOWN=3",
            "Only /CONTEXT=n, /LIMIT=n, /CASE=value, and /COUNT are supported."
        );

    remove_all(TEST_OUTPUT);

    if (!result) {
        (void)fprintf(
            stderr,
            "Command GREP parser regression test failed.\n"
        );
        return EXIT_FAILURE;
    }

    (void)fprintf(
        stderr,
        "Command GREP parser regression test passed.\n"
    );
    return EXIT_SUCCESS;
}
