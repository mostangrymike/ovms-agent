#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "llm_internal.h"
#include "project.h"

int openai_plan_clear_files(const char *plan_path);

static const char *openai_repair_rollback_text(void)
{
    if (openai_last_rollback == OPENAI_ROLLBACK_SUCCEEDED) {
        return "succeeded";
    }

    if (openai_last_rollback == OPENAI_ROLLBACK_FAILED) {
        return "failed";
    }

    return "not-required";
}

static void openai_repair_report(const agent_state *state,
                                 const char *outcome,
                                 unsigned int attempt)
{
    (void)puts("");
    (void)puts("Repair execution summary:");
    (void)printf("  Outcome:  %s\n",
                 outcome != NULL ? outcome : "unknown");
    (void)printf("  Attempts: %u of 2\n", attempt);

    if (openai_last_build_known) {
        (void)printf(
            "  Build:    status %d (%s)\n",
            openai_last_build_status,
            (openai_last_build_status & 1) != 0 ? "success" : "failure"
        );
    } else {
        (void)puts("  Build:    status unavailable");
    }

    (void)printf("  Rollback: %s\n",
                 openai_repair_rollback_text());
    (void)puts("");
    (void)puts("Final diff:");
    project_git_diff(state);
}

int openai_plan_is_noop_text(const char *text)
{
    static const char marker[] = "operation_count=0";
    const char *position;
    size_t marker_length;

    if (text == NULL) {
        return 0;
    }

    marker_length = strlen(marker);
    position = text;

    while ((position = strstr(position, marker)) != NULL) {
        int line_start;
        int line_end;

        line_start =
            position == text ||
            position[-1] == '\n' ||
            position[-1] == '\r';

        line_end =
            position[marker_length] == '\0' ||
            position[marker_length] == '\n' ||
            position[marker_length] == '\r';

        if (line_start && line_end) {
            return 1;
        }

        ++position;
    }

    return 0;
}

static const char *openai_test_repair_plan_text = NULL;
static const char *openai_test_repair_plan_text2 = NULL;
static int openai_test_repair_auto_approve = 0;
static unsigned int openai_test_repair_plan_index = 0U;
static char *openai_test_repair_prompt1 = NULL;
static char *openai_test_repair_prompt2 = NULL;

static void openai_test_clear_repair_prompts(void)
{
    free(openai_test_repair_prompt1);
    free(openai_test_repair_prompt2);
    openai_test_repair_prompt1 = NULL;
    openai_test_repair_prompt2 = NULL;
}

static void openai_test_capture_repair_prompt(unsigned int attempt,
                                              const char *prompt)
{
    char **destination;

    if (prompt == NULL) {
        return;
    }

    if (attempt == 1U) {
        destination = &openai_test_repair_prompt1;
    } else if (attempt == 2U) {
        destination = &openai_test_repair_prompt2;
    } else {
        return;
    }

    free(*destination);
    *destination = openai_duplicate_text(prompt);
}

const char *openai_test_get_repair_prompt(unsigned int attempt)
{
    if (attempt == 1U) {
        return openai_test_repair_prompt1;
    }

    if (attempt == 2U) {
        return openai_test_repair_prompt2;
    }

    return NULL;
}

void openai_test_set_repair_plan(const char *plan_text,
                                 int auto_approve)
{
    openai_test_repair_plan_text = plan_text;
    openai_test_repair_plan_text2 = NULL;
    openai_test_repair_auto_approve = auto_approve;
    openai_test_repair_plan_index = 0U;
    openai_test_clear_repair_prompts();
}

void openai_test_set_repair_plans(const char *plan_text1,
                                  const char *plan_text2,
                                  int auto_approve)
{
    openai_test_repair_plan_text = plan_text1;
    openai_test_repair_plan_text2 = plan_text2;
    openai_test_repair_auto_approve = auto_approve;
    openai_test_repair_plan_index = 0U;
    openai_test_clear_repair_prompts();
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

static char *openai_saved_plan_body(const char *saved_plan)
{
    static const char marker[] = "\n[plan]\n";
    const char *body;

    if (saved_plan == NULL) {
        return NULL;
    }

    body = strstr(saved_plan, marker);
    if (body == NULL) {
        return NULL;
    }

    body += strlen(marker);
    return openai_duplicate_text(body);
}

char *openai_build_goal_prompt(const char *goal,
                               const char *build_output)
{
    static const char introduction[] =
        "Create one deterministic transactional repair plan for the user's "
        "reported defect below. The baseline project build currently succeeds, "
        "so do not assume this is a compiler or linker failure. Inspect relevant "
        "project files and use the user's goal as the repair objective. The plan "
        "may contain multiple replace_text, replace_lines, or create operations "
        "when required for one coherent fix. Do not run a build, do not write "
        "files directly, and do not propose unrelated cleanup. The saved plan "
        "will be reviewed, approved once, applied atomically, rebuilt once, and "
        "rolled back automatically if the post-repair build fails.\n\n"
        "User goal:\n";
    static const char build_label[] =
        "\n\nPassing baseline build result:\n";
    char *combined_goal;
    size_t combined_size;

    if (goal == NULL || build_output == NULL) {
        return NULL;
    }

    combined_size =
        strlen(introduction) +
        strlen(goal) +
        strlen(build_label) +
        strlen(build_output) +
        1U;

    combined_goal = (char *)malloc(combined_size);
    if (combined_goal == NULL) {
        return NULL;
    }

    (void)strcpy(combined_goal, introduction);
    (void)strcat(combined_goal, goal);
    (void)strcat(combined_goal, build_label);
    (void)strcat(combined_goal, build_output);

    return combined_goal;
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

static char *openai_quote_plan_context(const char *plan)
{
    static const char prefix[] = "PRIOR> ";
    const char *source;
    char *quoted;
    char *destination;
    size_t line_count;
    size_t size;

    if (plan == NULL) {
        return NULL;
    }

    line_count = 1U;
    for (source = plan; *source != '\0'; ++source) {
        if (*source == '\n') {
            ++line_count;
        }
    }

    size =
        strlen(plan) +
        line_count * strlen(prefix) +
        1U;

    quoted = (char *)malloc(size);
    if (quoted == NULL) {
        return NULL;
    }

    source = plan;
    destination = quoted;

    (void)memcpy(destination, prefix, strlen(prefix));
    destination += strlen(prefix);

    while (*source != '\0') {
        *destination++ = *source;

        if (*source == '\n' && source[1] != '\0') {
            (void)memcpy(destination, prefix, strlen(prefix));
            destination += strlen(prefix);
        }

        ++source;
    }

    *destination = '\0';
    return quoted;
}

char *openai_build_retry_prompt(const char *goal,
                                const char *build_output,
                                const char *previous_plan)
{
    static const char introduction[] =
        "Create the second and final deterministic transactional repair "
        "plan. The first repair attempt was applied atomically, the rebuild "
        "still failed, and the complete first transaction was rolled back. "
        "Use the prior plan and the new build diagnostics as evidence. "
        "Do not repeat the same ineffective repair. Inspect current project "
        "files before finalizing a different coherent fix. Do not run a "
        "build or write files directly. This is the final allowed repair "
        "attempt.\n\nOriginal user goal:\n";
    static const char plan_label[] =
        "\n\nFirst repair plan that was rolled back:\n";
    static const char result_label[] =
        "\n\nBuild diagnostics after first repair attempt:\n";
    char *quoted_plan;
    char *prompt;
    size_t size;

    if (goal == NULL ||
        build_output == NULL ||
        previous_plan == NULL) {
        return NULL;
    }

    quoted_plan = openai_quote_plan_context(previous_plan);
    if (quoted_plan == NULL) {
        return NULL;
    }

    size =
        strlen(introduction) +
        strlen(goal) +
        strlen(plan_label) +
        strlen(quoted_plan) +
        strlen(result_label) +
        strlen(build_output) +
        1U;

    prompt = (char *)malloc(size);
    if (prompt == NULL) {
        free(quoted_plan);
        return NULL;
    }

    (void)strcpy(prompt, introduction);
    (void)strcat(prompt, goal);
    (void)strcat(prompt, plan_label);
    (void)strcat(prompt, quoted_plan);
    (void)strcat(prompt, result_label);
    (void)strcat(prompt, build_output);

    free(quoted_plan);
    return prompt;
}

void llm_agent_retry(agent_state *state, const char *goal)
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
    llm_agent_mode(state, combined_goal, 1, 1, OPENAI_WORKFLOW_FIX);

    free(combined_goal);
    free(build_output);
}

void llm_agent_repair(agent_state *state, const char *goal)
{
    char *build_output;
    char *combined_goal;
    char *previous_plan;
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
            "Current build succeeds. Continuing with the explicit repair goal."
        );
        openai_log_event(
            "AGENT/REPAIR",
            "passing_baseline",
            build_status
        );
    }

    previous_plan = NULL;

    for (attempt = 1U; attempt <= 2U; ++attempt) {
        if (attempt == 1U) {
            if ((build_status & 1) != 0) {
                combined_goal =
                    openai_build_goal_prompt(goal, build_output);
            } else {
                combined_goal =
                    openai_build_repair_prompt(goal, build_output);
            }
        } else {
            combined_goal =
                openai_build_retry_prompt(
                    goal,
                    build_output,
                    previous_plan
                );
        }

        if (combined_goal == NULL) {
            (void)puts("Insufficient memory for repair prompt.");
            free(build_output);
            openai_log_event("AGENT/REPAIR", "allocation_failed", 0);
            free(previous_plan);
            return;
        }

        if (using_test_plans) {
            openai_test_capture_repair_prompt(attempt, combined_goal);
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
                free(previous_plan);
                return;
            }

            if (!openai_plan_save(combined_goal, test_plan)) {
                (void)puts(
                    "AGENT/REPAIR test hook could not save deterministic plan."
                );
            }
        } else {
            llm_agent_plan(state, combined_goal);
        }

        free(combined_goal);
        free(build_output);
        build_output = NULL;

        if (!openai_plan_is_current(1)) {
            (void)puts(
                "AGENT/REPAIR stopped because no current deterministic plan "
                "was produced."
            );
            free(previous_plan);
            openai_log_event("AGENT/REPAIR", "plan_missing", (int)attempt);
            return;
        }

        {
            char *saved_plan;

            saved_plan =
                openai_read_text_file("OVMS_AGENT_PLAN.TXT");

            if (saved_plan != NULL &&
                openai_plan_is_noop_text(saved_plan)) {
                free(saved_plan);

                if (!openai_plan_clear_files(
                        "OVMS_AGENT_PLAN.TXT")) {
                    (void)puts(
                        "Warning: no-op repair plan could not be cleared."
                    );
                }

                (void)puts("No repair operations were proposed.");
                (void)puts(
                    "AGENT/REPAIR completed without modifying the project."
                );
                openai_log_event(
                    "AGENT/REPAIR",
                    "no_operations",
                    (int)attempt
                );
                free(previous_plan);
                return;
            }

            free(saved_plan);
        }

        if (attempt == 1U) {
            char *saved_plan;

            saved_plan =
                openai_read_text_file("OVMS_AGENT_PLAN.TXT");

            free(previous_plan);
            previous_plan =
                openai_saved_plan_body(saved_plan);
            free(saved_plan);

            if (previous_plan == NULL) {
                (void)puts(
                    "AGENT/REPAIR could not snapshot the first repair plan."
                );
                openai_log_event(
                    "AGENT/REPAIR",
                    "prior_plan_snapshot_failed",
                    1
                );
                return;
            }
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
            free(previous_plan);
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
            openai_log_repair_attempt(
                attempt,
                openai_approved_hash,
                openai_last_build_status,
                openai_last_rollback,
                "committed"
            );
            openai_log_event(
                "AGENT/REPAIR",
                "repair_succeeded",
                (int)attempt
            );
            openai_repair_report(state, "committed", attempt);
            free(previous_plan);
            return;
        }

        if (!openai_last_build_known ||
            openai_last_rollback != OPENAI_ROLLBACK_SUCCEEDED) {
            (void)puts(
                "AGENT/REPAIR stopped because the failed attempt did not "
                "complete a safe rollback."
            );
            openai_log_repair_attempt(
                attempt,
                openai_approved_hash,
                openai_last_build_status,
                openai_last_rollback,
                "unsafe"
            );
            openai_log_event(
                "AGENT/REPAIR",
                "unsafe_retry_state",
                (int)attempt
            );
            openai_repair_report(state, "unsafe-stop", attempt);
            free(previous_plan);
            return;
        }

        openai_log_repair_attempt(
            attempt,
            openai_approved_hash,
            openai_last_build_status,
            openai_last_rollback,
            "rolled_back"
        );

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
            openai_repair_report(state, "attempt-limit", attempt);
            free(previous_plan);
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
            free(previous_plan);
            return;
        }

        (void)puts(
            "First repair attempt failed and was rolled back. "
            "Attempt 2 will receive the first plan and new diagnostics."
        );

        openai_plan_approval_clear();
    }
}
