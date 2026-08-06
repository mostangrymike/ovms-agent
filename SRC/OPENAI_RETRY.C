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

void openai_agent_repair(agent_state *state, const char *goal)
{
    static const char introduction[] =
        "Create one deterministic transactional repair plan for the failed "
        "build below. Inspect relevant project files before finalizing the "
        "plan. The plan may contain multiple replace_text, replace_lines, "
        "or create operations when they are required for one coherent fix. "
        "Do not run a build, do not write files directly, and do not propose "
        "unrelated cleanup. The saved plan will be reviewed, approved once, "
        "applied atomically, rebuilt once, and rolled back automatically if "
        "the build still fails.\n\nUser goal:\n";
    static const char log_label[] =
        "\n\nCurrent failed build result:\n";
    char *build_output;
    char *combined_goal;
    size_t combined_size;
    int build_status;

    if (state == NULL ||
        state->project_root == NULL ||
        *state->project_root == '\0') {
        (void)puts("OVMS_AGENT_ROOT is not defined.");
        return;
    }

    if (goal == NULL || *goal == '\0') {
        (void)puts("Usage: AGENT/REPAIR goal");
        return;
    }

    if (!state->write_enabled) {
        (void)puts(
            "AGENT/REPAIR requires write access. "
            "Define OVMS_AGENT_WRITE_ENABLED as YES and restart."
        );
        return;
    }

    if (!state->dcl_enabled) {
        (void)puts(
            "AGENT/REPAIR requires DCL execution. "
            "Define OVMS_AGENT_DCL_ENABLED as YES and restart."
        );
        return;
    }

    if (openai_path_is_sensitive(goal)) {
        (void)puts(
            "Request denied because it references a sensitive path."
        );
        return;
    }

    openai_last_rollback = OPENAI_ROLLBACK_NONE;
    openai_log_event("AGENT/REPAIR", "start", 0);

    (void)puts("Checking the current project build before repair...");

    build_output = execute_run_build_tool(&build_status);

    if (build_output == NULL) {
        (void)puts("Unable to capture the current build result.");
        openai_log_event("AGENT/REPAIR", "build_capture_failed", 0);
        return;
    }

    (void)printf(
        "Tool executed: run_build [%s, status %d]\n",
        (build_status & 1) != 0 ? "success" : "failure",
        build_status
    );

    if ((build_status & 1) != 0) {
        (void)puts(
            "Current build succeeds. No repair is required."
        );
        free(build_output);
        openai_log_event("AGENT/REPAIR", "build_already_passes", build_status);
        return;
    }

    combined_size =
        strlen(introduction) +
        strlen(goal) +
        strlen(log_label) +
        strlen(build_output) +
        1U;

    combined_goal = (char *)malloc(combined_size);

    if (combined_goal == NULL) {
        (void)puts("Insufficient memory for repair prompt.");
        free(build_output);
        openai_log_event("AGENT/REPAIR", "allocation_failed", 0);
        return;
    }

    (void)strcpy(combined_goal, introduction);
    (void)strcat(combined_goal, goal);
    (void)strcat(combined_goal, log_label);
    (void)strcat(combined_goal, build_output);

    (void)puts(
        "Current build fails. Creating one transactional repair plan..."
    );

    openai_agent_plan(state, combined_goal);

    free(combined_goal);
    free(build_output);

    if (!openai_plan_is_current(1)) {
        (void)puts(
            "AGENT/REPAIR stopped because no current deterministic plan "
            "was produced."
        );
        openai_log_event("AGENT/REPAIR", "plan_missing", 0);
        return;
    }

    /*
     * openai_plan_approve displays the complete saved plan and obtains the
     * workflow's single user confirmation.
     */
    openai_plan_approve();

    if (!openai_plan_approved) {
        (void)puts("AGENT/REPAIR cancelled before any project write.");
        openai_log_event("AGENT/REPAIR", "approval_declined", 0);
        return;
    }

    (void)puts(
        "Applying the approved repair as one transaction and rebuilding..."
    );

    /*
     * The existing saved-plan executor stages all operations in one edit_txn,
     * runs exactly one controlled build, commits on success, and rolls the
     * complete transaction back on failure.
     */
    openai_plan_execute(state);
}
