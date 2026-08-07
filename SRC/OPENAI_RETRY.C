#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "openai_internal.h"



static const char *openai_test_repair_plan_text = NULL;
static const char *openai_test_repair_plan_text2 = NULL;
static int openai_test_repair_auto_approve = 0;
static unsigned int openai_test_repair_plan_index = 0U;

void openai_test_set_repair_plan(const char *plan_text,
                                 int auto_approve)
{
    openai_test_repair_plan_text = plan_text;
    openai_test_repair_plan_text2 = NULL;
    openai_test_repair_auto_approve = auto_approve;
    openai_test_repair_plan_index = 0U;
}

void openai_test_set_repair_plans(const char *plan_text1,
                                  const char *plan_text2,
                                  int auto_approve)
{
    openai_test_repair_plan_text = plan_text1;
    openai_test_repair_plan_text2 = plan_text2;
    openai_test_repair_auto_approve = auto_approve;
    openai_test_repair_plan_index = 0U;
}

static const char *openai_test_next_repair_plan(void)
{
    const char *plan_text;

    if (openai_test_repair_plan_text == NULL) {
        return NULL;
    }

    if (openai_test_repair_plan_index == 0U) {
        plan_text = openai_test_repair_plan_text;
    } else {
        plan_text = openai_test_repair_plan_text2;
    }

    ++openai_test_repair_plan_index;
    return plan_text;
}

char *openai_build_repair_prompt(const char *goal,
                                 const char *build_output)
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
    char *combined_goal;
    size_t combined_size;

    if (goal == NULL || build_output == NULL) {
        return NULL;
    }

    combined_size =
        strlen(introduction) +
        strlen(goal) +
        strlen(log_label) +
        strlen(build_output) +
        1U;

    combined_goal = (char *)malloc(combined_size);

    if (combined_goal == NULL) {
        return NULL;
    }

    (void)strcpy(combined_goal, introduction);
    (void)strcat(combined_goal, goal);
    (void)strcat(combined_goal, log_label);
    (void)strcat(combined_goal, build_output);

    return combined_goal;
}

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
    char *build_output;
    char *combined_goal;
    const char *test_plan;
    unsigned int attempt;
    int build_status;
    int using_test_plans;

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

    using_test_plans =
        openai_test_repair_plan_text != NULL;

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
        openai_log_event(
            "AGENT/REPAIR",
            "build_already_passes",
            build_status
        );
        return;
    }

    for (attempt = 1U; attempt <= 2U; ++attempt) {
        combined_goal =
            openai_build_repair_prompt(goal, build_output);

        if (combined_goal == NULL) {
            (void)puts("Insufficient memory for repair prompt.");
            free(build_output);
            openai_log_event("AGENT/REPAIR", "allocation_failed", 0);
            return;
        }

        (void)printf(
            "Repair attempt %u of 2: creating transactional plan...\n",
            attempt
        );

        test_plan = NULL;

        if (using_test_plans) {
            test_plan = openai_test_next_repair_plan();

            if (test_plan == NULL) {
                free(combined_goal);
                free(build_output);
                (void)puts(
                    "AGENT/REPAIR deterministic test plan sequence ended."
                );
                openai_log_event(
                    "AGENT/REPAIR",
                    "test_plan_sequence_ended",
                    (int)attempt
                );
                return;
            }

            if (!openai_plan_save(combined_goal, test_plan)) {
                (void)puts(
                    "AGENT/REPAIR test hook could not save deterministic plan."
                );
            }
        } else {
            openai_agent_plan(state, combined_goal);
        }

        free(combined_goal);
        free(build_output);
        build_output = NULL;

        if (!openai_plan_is_current(1)) {
            (void)puts(
                "AGENT/REPAIR stopped because no current deterministic plan "
                "was produced."
            );
            openai_log_event("AGENT/REPAIR", "plan_missing", (int)attempt);
            return;
        }

        if (using_test_plans &&
            openai_test_repair_auto_approve) {
            (void)openai_plan_approve_file("OVMS_AGENT_PLAN.TXT");
        } else {
            openai_plan_approve();
        }

        if (!openai_plan_approved) {
            (void)puts(
                "AGENT/REPAIR cancelled before any project write."
            );
            openai_log_event(
                "AGENT/REPAIR",
                "approval_declined",
                (int)attempt
            );
            return;
        }

        (void)printf(
            "Repair attempt %u of 2: applying transaction and rebuilding...\n",
            attempt
        );

        /*
         * Clear prior workflow evidence so only this transaction's result
         * can authorize either success or a bounded retry.
         */
        openai_last_build_known = 0;
        openai_last_rollback = OPENAI_ROLLBACK_NONE;

        openai_plan_execute(state);

        if (openai_last_build_known &&
            (openai_last_build_status & 1) != 0) {
            (void)printf(
                "AGENT/REPAIR succeeded on attempt %u.\n",
                attempt
            );
            openai_log_event(
                "AGENT/REPAIR",
                "repair_succeeded",
                (int)attempt
            );
            return;
        }

        if (!openai_last_build_known ||
            openai_last_rollback != OPENAI_ROLLBACK_SUCCEEDED) {
            (void)puts(
                "AGENT/REPAIR stopped because the failed attempt did not "
                "complete a safe rollback."
            );
            openai_log_event(
                "AGENT/REPAIR",
                "unsafe_retry_state",
                (int)attempt
            );
            return;
        }

        if (attempt == 2U) {
            (void)puts(
                "AGENT/REPAIR reached the two-attempt limit. "
                "The final failed transaction was rolled back."
            );
            openai_log_event(
                "AGENT/REPAIR",
                "retry_limit_reached",
                2
            );
            return;
        }

        build_output =
            openai_read_text_file("OVMS_AGENT_FAILED_BUILD.TXT");

        if (build_output == NULL) {
            (void)puts(
                "AGENT/REPAIR cannot continue because the failed rebuild "
                "diagnostics could not be reloaded."
            );
            openai_log_event(
                "AGENT/REPAIR",
                "retry_diagnostics_missing",
                (int)attempt
            );
            return;
        }

        (void)puts(
            "First repair attempt failed and was rolled back. "
            "Using the new build diagnostics for attempt 2."
        );

        openai_plan_approval_clear();
    }
}
