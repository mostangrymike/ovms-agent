#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "openai_internal.h"

int command_line_complete(const char *input,
                          size_t input_size,
                          int reached_eof)
{
    (void)input; (void)input_size; (void)reached_eof; return 0;
}

int command_read_stream(FILE *stream,
                        char *input,
                        size_t input_size)
{
    (void)stream; (void)input; (void)input_size; return 0;
}

int main(void)
{
    agent_state state;
    char output[32768];

    (void)memset(&state, 0, sizeof(state));
    state.project_root = ".";

    openai_test_git_data(
        " M SRC/OPENAI_AGENT.C\n"
        "?? SRC/OPENAI_GIT_CONTEXT.C",
        "diff --git a/SRC/OPENAI_AGENT.C b/SRC/OPENAI_AGENT.C\n"
        "+    git context enabled"
    );

    if (!openai_git_refresh(&state)) {
        (void)puts("M234 failed: Git refresh.");
        return EXIT_FAILURE;
    }

    if (!openai_git_status_text(
            &state, output, sizeof(output)) ||
        strstr(output, "Changed paths: 2") == NULL ||
        strstr(output, "OPENAI_AGENT.C") == NULL ||
        strstr(output, "OPENAI_GIT_CONTEXT.C") == NULL) {
        (void)puts("M234 failed: Git status context.");
        return EXIT_FAILURE;
    }

    if (!openai_git_diff_text(
            &state, output, sizeof(output)) ||
        strstr(output, "git context enabled") == NULL) {
        (void)puts("M234 failed: Git diff context.");
        return EXIT_FAILURE;
    }

    if (!openai_git_changed_text(
            &state, output, sizeof(output)) ||
        strstr(output, "Count: 2") == NULL) {
        (void)puts("M234 failed: changed path view.");
        return EXIT_FAILURE;
    }

    if (!openai_git_compose(
            &state,
            "Review the current edits.",
            output,
            sizeof(output)) ||
        strstr(output, "GIT WORKING TREE") == NULL ||
        strstr(output, "UNSTAGED DIFF") == NULL ||
        strstr(output, "Review the current edits.") == NULL) {
        (void)puts("M234 failed: Git context composition.");
        return EXIT_FAILURE;
    }

    if (!openai_parity_text(output, sizeof(output)) ||
        strstr(output, "Git state context:     available") == NULL ||
        strstr(output, "Git diff awareness:    available") == NULL) {
        (void)puts("M234 failed: parity status.");
        return EXIT_FAILURE;
    }

    openai_test_git_data(NULL, NULL);

    (void)puts("Git-aware autonomous context bundle test passed.");
    return EXIT_SUCCESS;
}
