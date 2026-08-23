/* M268 bounded end-to-end repair orchestration.
 *
 * This layer deliberately reuses the mature build, REPAIR/PLAN, saved-plan
 * approval/execution, transactional rollback, repair history, Git context,
 * and AGENT/AUTO/LIMITS machinery.  It adds sequencing and terminal-state
 * reporting only.
 */
#include "LLM_AUTO.H"
#include "LLM_REPAIR.H"

#define M268_REVIEW_FILE "OVMS_AGENT_AUTOPILOT_REVIEW.TXT"
#define M268_HISTORY_FILE "OVMS_AGENT_AUTOPILOT_HISTORY.TXT"
#define M268_TRACE_FILE "OVMS_AGENT_AUTOPILOT_PLANS.TXT"
#define M268_PLAN_FILE "OVMS_AGENT_PLAN.TXT"

#define M268_TERM_CLEAN "CLEAN_BUILD"
#define M268_TERM_BUDGET "RETRY_BUDGET_EXHAUSTED"
#define M268_TERM_GUARD "GOAL_GUARD_REJECTED"

int llm_execute_count_ops(const char *plan_text,
                          unsigned int *operation_count_out);

#include "LLM_AUTOPILOT_GUARD.INC"

static int m268_first_repair_plan(agent_state *state, const char *goal)
{
    char *build_output;
    char *repair_prompt;
    int current;

    build_output = llm_read_text_file(LLM_BUILD_LOG_FILE);
    if (build_output == NULL) {
        return 0;
    }

    repair_prompt = llm_build_repair_prompt(goal, build_output);
    free(build_output);
    if (repair_prompt == NULL) {
        return 0;
    }

    /* Reuse the exact first-attempt deterministic planning path used by
     * AGENT/REPAIR.  REPAIR/PLAN itself deliberately requires evidence from
     * an already-failed transaction, so it becomes the retry planner after
     * the first candidate has failed and rolled back.
     */
    llm_agent_plan(state, repair_prompt);
    free(repair_prompt);

    current = llm_plan_is_current(1);
    return current;
}

static unsigned int m268_plan_write_count(void)
{
    char *saved;
    unsigned int count;

    saved = llm_read_text_file(M268_PLAN_FILE);
    if (saved == NULL) {
        return 0U;
    }

    count = 0U;
    if (!llm_execute_count_ops(saved, &count)) {
        count = 0U;
    }

    free(saved);
    return count;
}

static void m268_trace_reset(const char *goal)
{
    FILE *file;

    while (remove(M268_TRACE_FILE) == 0) {
    }

    file = fopen(M268_TRACE_FILE, "w");
    if (file == NULL) {
        return;
    }

    (void)fprintf(
        file,
        "OVMS Agent AUTOPILOT candidate trace\n"
        "Goal: %s\n\n",
        goal != NULL ? goal : ""
    );
    (void)fclose(file);
}

static void m268_trace_candidate(unsigned int candidate,
                                 unsigned long plan_hash)
{
    FILE *file;
    char *saved;

    saved = llm_read_text_file(M268_PLAN_FILE);
    if (saved == NULL) {
        return;
    }

    file = fopen(M268_TRACE_FILE, "a");
    if (file != NULL) {
        (void)fprintf(
            file,
            "===== CANDIDATE %u PLAN %08lX =====\n",
            candidate,
            plan_hash
        );
        (void)fputs(saved, file);
        if (*saved != '\0' && saved[strlen(saved) - 1U] != '\n') {
            (void)fputc('\n', file);
        }
        (void)fputc('\n', file);
        (void)fclose(file);
    }

    free(saved);
}

static void m268_review_write(const agent_state *state,
                              const char *goal,
                              const char *terminal,
                              const char *detail,
                              unsigned int candidates)
{
    FILE *file;
    char *history;
    char *build_log;
    char *diff_text;
    char *candidate_trace;
    size_t diff_size;

    history = NULL;
    build_log = NULL;
    diff_text = NULL;
    candidate_trace = NULL;
    diff_size = 32768U;

    (void)llm_repair_export_file(M268_HISTORY_FILE, 1);
    history = llm_read_text_file(M268_HISTORY_FILE);
    candidate_trace = llm_read_text_file(M268_TRACE_FILE);

    build_log = llm_read_text_file(LLM_BUILD_LOG_FILE);
    if (build_log == NULL) {
        build_log = llm_read_text_file("OVMS_AGENT_FAILED_BUILD.TXT");
    }

    diff_text = (char *)malloc(diff_size);
    if (diff_text != NULL) {
        diff_text[0] = '\0';
        if (!llm_git_refresh(state) ||
            !llm_git_diff_text(state, diff_text, diff_size)) {
            (void)strcpy(diff_text, "Git diff unavailable.\n");
        }
    }

    file = fopen(M268_REVIEW_FILE, "w");
    if (file == NULL) {
        (void)puts("Unable to create AUTOPILOT review artifact.");
        free(history);
        free(build_log);
        free(diff_text);
        free(candidate_trace);
        return;
    }

    (void)fprintf(
        file,
        "OVMS Agent M268 AUTOPILOT review\n"
        "================================\n"
        "Terminal state: %s\n"
        "Repair candidates applied: %u\n"
        "Goal: %s\n"
        "Detail: %s\n"
        "Push performed: no\n"
        "\n"
        "REPAIR CANDIDATE PLANS / EXACT OLD-NEW EDITS\n"
        "--------------------------------------------\n",
        terminal != NULL ? terminal : "UNKNOWN",
        candidates,
        goal != NULL ? goal : "",
        detail != NULL && *detail != '\0' ? detail : "none"
    );

    if (candidate_trace != NULL) {
        (void)fputs(candidate_trace, file);
    } else {
        (void)fputs("No repair candidate was applied.\n", file);
    }

    (void)fputs(
        "\nCURRENT WORKING-TREE DIFF\n"
        "-------------------------\n",
        file
    );
    if (diff_text != NULL) {
        (void)fputs(diff_text, file);
    } else {
        (void)fputs("Git diff unavailable.\n", file);
    }

    (void)fputs(
        "\nBUILD LOG / LAST BUILD EVIDENCE\n"
        "-------------------------------\n",
        file
    );
    if (build_log != NULL) {
        (void)fputs(build_log, file);
    } else {
        (void)fputs("Build log unavailable.\n", file);
    }

    (void)fputs(
        "\nREPAIR HISTORY EXPORT\n"
        "---------------------\n",
        file
    );
    if (history != NULL) {
        (void)fputs(history, file);
    } else {
        (void)fputs("Repair history unavailable.\n", file);
    }

    if (fclose(file) != 0) {
        (void)puts("Warning: AUTOPILOT review artifact close failed.");
    }

    free(history);
    free(build_log);
    free(diff_text);
    free(candidate_trace);
}

static void m268_terminal(const agent_state *state,
                          const char *goal,
                          const char *terminal,
                          const char *auto_reason,
                          const char *detail,
                          unsigned int candidates)
{
    llm_auto_outer_end(auto_reason);
    m268_review_write(state, goal, terminal, detail, candidates);

    (void)puts("");
    (void)printf("AUTOPILOT TERMINAL: %s\n", terminal);
    if (detail != NULL && *detail != '\0') {
        (void)printf("Reason: %s\n", detail);
    }
    (void)printf("Review artifact: %s\n", M268_REVIEW_FILE);
    (void)puts(
        "Review the artifact before any later PUSH or other FULL-gated action."
    );
}

void llm_agent_autopilot(agent_state *state, const char *goal)
{
    char guard_reason[256];
    const char *terminal;
    const char *auto_reason;
    const char *detail;
    unsigned int candidates;
    unsigned int write_count;
    unsigned long plan_hash;

    if (state == NULL ||
        state->project_root == NULL ||
        *state->project_root == '\0') {
        (void)puts("OVMS_AGENT_ROOT is not defined.");
        return;
    }

    if (goal == NULL || *goal == '\0') {
        (void)puts("Usage: AGENT/AUTOPILOT goal");
        return;
    }

    if (!state->write_enabled || !state->dcl_enabled) {
        (void)puts(
            "AGENT/AUTOPILOT requires the existing local write and controlled "
            "DCL build gates to be enabled."
        );
        return;
    }

    if (strcmp(llm_approval_name(), "autopilot") != 0) {
        (void)puts(
            "AGENT/AUTOPILOT requires: AGENT/APPROVAL/SET autopilot"
        );
        (void)puts(
            "FULL does not enable AUTOPILOT; FULL-gated actions remain manual."
        );
        return;
    }

    candidates = 0U;
    terminal = M268_TERM_BUDGET;
    auto_reason = "budget-exhausted";
    detail = "Autonomous turn/write budget exhausted before a clean build.";
    guard_reason[0] = '\0';

    llm_log_event("AGENT/AUTOPILOT", "start", 0);
    llm_auto_outer_begin();
    m268_trace_reset(goal);

    for (;;) {
        /* AGENT/BUILD performs the initial request and its follow-up analysis. */
        if (!llm_auto_take_turns(2U)) {
            break;
        }

        llm_last_build_known = 0;
        llm_last_rollback = LLM_ROLLBACK_NONE;
        llm_agent_build(state, goal);

        if (llm_last_build_known &&
            (llm_last_build_status & 1) != 0) {
            terminal = M268_TERM_CLEAN;
            auto_reason = "clean-build";
            detail = "Controlled build completed successfully.";
            break;
        }

        if (!llm_last_build_known) {
            detail = "Controlled build did not produce usable build evidence.";
            break;
        }

        if (llm_auto_budget_exhausted()) {
            break;
        }

        /* Never reuse a plan from a prior outer iteration. */
        (void)llm_plan_clear_files(M268_PLAN_FILE);

        if (candidates == 0U) {
            (void)puts(
                "AUTOPILOT: build failed; invoking existing first-repair "
                "deterministic planner."
            );
            if (!m268_first_repair_plan(state, goal)) {
                detail =
                    "Initial repair planning produced no current executable "
                    "candidate.";
                break;
            }
        } else {
            (void)puts(
                "AUTOPILOT: build still fails; invoking existing "
                "AGENT/REPAIR/PLAN."
            );
            llm_repair_plan(state);
            if (!llm_plan_is_current(1)) {
                detail =
                    "AGENT/REPAIR/PLAN produced no current executable "
                    "candidate.";
                break;
            }
        }

        if (llm_auto_budget_exhausted()) {
            detail =
                "Repair planning consumed the remaining autonomous turn "
                "budget before any candidate write.";
            break;
        }

        if (!m268_goal_guard_plan(
                goal, guard_reason, sizeof(guard_reason))) {
            terminal = M268_TERM_GUARD;
            auto_reason = "goal-guard";
            detail = guard_reason[0] != '\0' ?
                guard_reason : "Repair candidate rejected by goal guard.";
            llm_log_event("AGENT/AUTOPILOT", "goal_guard_rejected", 0);
            break;
        }

        write_count = m268_plan_write_count();
        if (write_count == 0U) {
            detail = "Repair candidate contains no countable executable writes.";
            break;
        }

        if (!llm_auto_take_writes(write_count)) {
            detail = "No autonomous write budget remains for this repair plan.";
            break;
        }

        if (!llm_plan_approve_file(M268_PLAN_FILE)) {
            detail = "Repair candidate failed existing plan integrity approval.";
            break;
        }

        plan_hash = llm_approved_hash;
        ++candidates;
        m268_trace_candidate(candidates, plan_hash);
        (void)printf(
            "AUTOPILOT: applying repair candidate %u (%u write%s) and rebuilding...\n",
            candidates,
            write_count,
            write_count == 1U ? "" : "s"
        );

        llm_last_build_known = 0;
        llm_last_rollback = LLM_ROLLBACK_NONE;
        llm_plan_execute(state);

        if (llm_last_build_known &&
            (llm_last_build_status & 1) != 0) {
            llm_log_repair_attempt(
                1U, plan_hash, llm_last_build_status,
                llm_last_rollback, "committed");
            terminal = M268_TERM_CLEAN;
            auto_reason = "clean-build";
            detail = "Repair candidate committed and controlled rebuild passed.";
            break;
        }

        if (llm_last_build_known &&
            llm_last_rollback == LLM_ROLLBACK_SUCCEEDED) {
            llm_log_repair_attempt(
                1U, plan_hash, llm_last_build_status,
                llm_last_rollback, "rolled_back");
            llm_log_event(
                "AGENT/AUTOPILOT", "candidate_rolled_back", (int)candidates);

            if (llm_auto_budget_exhausted()) {
                detail = "Repair candidate failed safely; retry budget is exhausted.";
                break;
            }

            (void)puts(
                "AUTOPILOT: candidate failed and rolled back; starting next loop."
            );
            continue;
        }

        llm_log_repair_attempt(
            1U, plan_hash, llm_last_build_status,
            llm_last_rollback, "unsafe");
        detail =
            "Repair execution did not leave a safely retryable state; "
            "AUTOPILOT stopped without another write.";
        break;
    }

    llm_log_event(
        "AGENT/AUTOPILOT",
        strcmp(terminal, M268_TERM_CLEAN) == 0 ? "clean_build" :
        (strcmp(terminal, M268_TERM_GUARD) == 0 ?
            "goal_guard_rejected" : "retry_budget_exhausted"),
        (int)candidates
    );

    m268_terminal(
        state, goal, terminal, auto_reason, detail, candidates);
}

/* Deterministic M268 regression hook.  It exercises the same terminal
 * finalizer and review writer used by production without invoking a model or
 * touching project source files.
 */
int llm_m268_terminal_test(const agent_state *state,
                           unsigned int terminal_index)
{
    const char *terminal;
    const char *auto_reason;

    if (terminal_index == 1U) {
        terminal = M268_TERM_CLEAN;
        auto_reason = "clean-build";
    } else if (terminal_index == 2U) {
        terminal = M268_TERM_BUDGET;
        auto_reason = "budget-exhausted";
    } else if (terminal_index == 3U) {
        terminal = M268_TERM_GUARD;
        auto_reason = "goal-guard";
    } else {
        return 0;
    }

    llm_auto_outer_begin();
    m268_terminal(
        state,
        "M268 deterministic terminal test",
        terminal,
        auto_reason,
        "deterministic terminal regression",
        0U
    );
    return 1;
}
