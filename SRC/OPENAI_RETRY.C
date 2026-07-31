#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "openai_internal.h"

void openai_agent_retry(agent_state *state, const char *goal)
{
    static const char introduction[] =
        "The current project build has just failed. This is a fresh "
        "supervised repair attempt. Use the current build diagnostics below "
        "as evidence. Inspect the current source before proposing one new "
        "smallest patch. Apply at most one replace_text or replace_lines "
        "operation and "
        "then run the controlled build. Do not propose unrelated cleanup."
        "\n\nUser goal:\n";
    static const char log_label[] =
        "\n\nCurrent failed build result:\n";
    char *build_output;
    char *combined_goal;
    size_t combined_size;
    int build_status;

    openai_last_workflow = OPENAI_WORKFLOW_RETRY;
    openai_log_event(
        openai_workflow_name(openai_last_workflow),
        "start",
        0
    );
    openai_last_rollback = OPENAI_ROLLBACK_NONE;

    if (state == NULL ||
        state->project_root == NULL ||
        *state->project_root == '\0') {
        (void)puts("OVMS_AGENT_ROOT is not defined.");
        return;
    }

    if (goal == NULL || *goal == '\0') {
        (void)puts("Usage: AGENT/RETRY goal");
        return;
    }

    if (openai_path_is_sensitive(goal)) {
        (void)puts(
            "Request denied because it references a sensitive path."
        );
        return;
    }

    (void)puts("Checking the current project build before retrying...");

    build_output = execute_run_build_tool(&build_status);

    if (build_output == NULL) {
        (void)puts("Unable to capture the current build result.");
        return;
    }

    (void)printf(
        "Tool executed: run_build [%s, status %d]\n",
        (build_status & 1) != 0 ? "success" : "failure",
        build_status
    );

    if ((build_status & 1) != 0) {
        (void)puts(
            "Current build succeeds. The previous failure has already "
            "been resolved or rolled back; no repair is required."
        );
        free(build_output);
        return;
    }

    combined_size =
        strlen(introduction) +
        strlen(goal) +
        strlen(log_label) +
        strlen(build_output) +
        1U;

    combined_goal = malloc(combined_size);

    if (combined_goal == NULL) {
        (void)puts("Insufficient memory for retry prompt.");
        free(build_output);
        return;
    }

    (void)strcpy(combined_goal, introduction);
    (void)strcat(combined_goal, goal);
    (void)strcat(combined_goal, log_label);
    (void)strcat(combined_goal, build_output);

    (void)puts(
        "Current build still fails. Starting a fresh supervised retry..."
    );

    /*
     * A retry remains one patch and one post-patch controlled build. The
     * existing supervised mode enforces local confirmation and rollback.
     */
    openai_agent_mode(state, combined_goal, 1, 1, OPENAI_WORKFLOW_FIX);

    free(combined_goal);
    free(build_output);
}
