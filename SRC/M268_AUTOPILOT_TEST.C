#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "llm_internal.h"
#include "LLM_AUTO.H"
#include "command_internal.h"

#define M268_TEST_PLAN "OVMS_AGENT_PLAN.TXT"
#define M268_TEST_LOG "M268_ACTIVITY.TMP"
#define M268_REVIEW_FILE "OVMS_AGENT_AUTOPILOT_REVIEW.TXT"
#define M268_HISTORY_FILE "OVMS_AGENT_AUTOPILOT_HISTORY.TXT"
#define M268_TRACE_FILE "OVMS_AGENT_AUTOPILOT_PLANS.TXT"

int llm_m268_policy_floor(void);
int llm_m268_guard_test(const char *goal,
                        char *reason,
                        size_t reason_size);
int llm_m268_terminal_test(const agent_state *state,
                           unsigned int terminal_index);
int llm_net_allow_once(const char *domain);

/* Link-only stubs used by the focused LLM test executable.  Production
 * command input is supplied by the normal OVMS Agent image.
 */
int command_line_complete(const char *input,
                          size_t input_size,
                          int reached_eof)
{
    (void)input;
    (void)input_size;
    (void)reached_eof;
    return 0;
}

int command_read_stream(FILE *stream,
                        char *input,
                        size_t input_size)
{
    (void)stream;
    (void)input;
    (void)input_size;
    return 0;
}

static unsigned int m268_gh_calls = 0U;
static unsigned int m268_mcp_calls = 0U;

static int m268_gh_exec(const char *operation,
                        const char *arguments,
                        char *result,
                        size_t result_size,
                        void *context)
{
    int written;

    (void)context;
    ++m268_gh_calls;
    written = snprintf(
        result,
        result_size,
        "M268 mock GitHub execution: %s %s\n",
        operation != NULL ? operation : "",
        arguments != NULL ? arguments : ""
    );
    return written >= 0 && (size_t)written < result_size;
}

static int m268_mcp_exec(const char *target,
                         const char *tool,
                         const char *arguments,
                         char *result,
                         size_t result_size,
                         void *context)
{
    int written;

    (void)context;
    ++m268_mcp_calls;
    written = snprintf(
        result,
        result_size,
        "M268 mock MCP execution: %s %s %s\n",
        target != NULL ? target : "",
        tool != NULL ? tool : "",
        arguments != NULL ? arguments : ""
    );
    return written >= 0 && (size_t)written < result_size;
}

static int contains(const char *text, const char *needle)
{
    return text != NULL && needle != NULL && strstr(text, needle) != NULL;
}

static void remove_all(const char *path)
{
    while (remove(path) == 0) {
    }
}

static int write_text(const char *path, const char *text)
{
    FILE *file;

    file = fopen(path, "w");
    if (file == NULL) {
        return 0;
    }

    if (fputs(text, file) == EOF) {
        (void)fclose(file);
        return 0;
    }

    return fclose(file) == 0;
}

static int file_contains(const char *path, const char *needle)
{
    FILE *file;
    char line[1024];
    int found;

    file = fopen(path, "r");
    if (file == NULL) {
        return 0;
    }

    found = 0;
    while (fgets(line, sizeof(line), file) != NULL) {
        if (strstr(line, needle) != NULL) {
            found = 1;
            break;
        }
    }

    (void)fclose(file);
    return found;
}

static int terminal_review_ok(const agent_state *state,
                              unsigned int terminal_index,
                              const char *terminal_name)
{
    if (!llm_m268_terminal_test(state, terminal_index)) {
        return 0;
    }

    return
        file_contains(M268_REVIEW_FILE, terminal_name) &&
        file_contains(M268_REVIEW_FILE, "M268_BUILD_MARKER") &&
        file_contains(M268_REVIEW_FILE, "M268_DIFF_MARKER") &&
        file_contains(M268_REVIEW_FILE, "M268_PLAN_MARKER") &&
        file_contains(M268_REVIEW_FILE, "12345678") &&
        file_contains(M268_REVIEW_FILE, "Push performed: no");
}

int main(void)
{
    char output[4096];
    char reason[256];
    agent_state dcl_state;
    unsigned long dcl_status;

    remove_all(M268_TEST_PLAN);
    llm_reset_approval();
    llm_auto_test_limits(4U, 2U);
    llm_auto_reset();

    if (!llm_set_approval("autopilot") ||
        strcmp(llm_approval_name(), "autopilot") != 0 ||
        !llm_m268_policy_floor()) {
        (void)puts("M268 failed: AUTOPILOT approval safety floor.");
        return EXIT_FAILURE;
    }

    if (!llm_approval_text(output, sizeof(output)) ||
        !contains(output, "local workspace writes") ||
        !contains(output, "FULL-gated actions remain manual")) {
        (void)puts("M268 failed: AUTOPILOT approval disclosure.");
        return EXIT_FAILURE;
    }

    /* Exercise real FULL-gated entry points.  The mock executors must never
     * be reached while the AUTOPILOT overlay is active.
     */
    m268_gh_calls = 0U;
    if (!llm_gh_run_text(
            "push", "origin main", m268_gh_exec, NULL,
            output, sizeof(output)) ||
        !contains(output, "FULL approval policy is required") ||
        m268_gh_calls != 0U) {
        (void)puts("M268 failed: AUTOPILOT crossed GitHub PUSH FULL gate.");
        return EXIT_FAILURE;
    }

    m268_gh_calls = 0U;
    if (!llm_gh_run_text(
            "issues", "list open", m268_gh_exec, NULL,
            output, sizeof(output)) ||
        !contains(output, "FULL approval policy is required") ||
        m268_gh_calls != 0U) {
        (void)puts("M268 failed: AUTOPILOT crossed GitHub service FULL gate.");
        return EXIT_FAILURE;
    }

    m268_mcp_calls = 0U;
    if (!llm_mcp_execute_text(
            "docs|stdio|@MCP_DOCS",
            "docs search RMS",
            m268_mcp_exec,
            NULL,
            output,
            sizeof(output)) ||
        !contains(output, "FULL approval policy is required") ||
        m268_mcp_calls != 0U) {
        (void)puts("M268 failed: AUTOPILOT crossed MCP FULL gate.");
        return EXIT_FAILURE;
    }

    if (llm_net_allow_once("autopilot.example.test")) {
        (void)puts("M268 failed: AUTOPILOT authorized network exception.");
        return EXIT_FAILURE;
    }

    (void)memset(&dcl_state, 0, sizeof(dcl_state));
    dcl_status = 0UL;
    output[0] = '\0';
    if (command_dcl_exec(
            &dcl_state,
            "SHOW TIME",
            output,
            sizeof(output),
            &dcl_status) ||
        !contains(output, "full approval policy required")) {
        (void)puts("M268 failed: AUTOPILOT crossed arbitrary DCL FULL gate.");
        return EXIT_FAILURE;
    }

    /* A failed multi-write reservation must be all-or-nothing. */
    llm_auto_outer_begin();
    if (!llm_auto_take_writes(1U) ||
        llm_auto_take_writes(2U) ||
        llm_m268_write_count() != 1U) {
        (void)puts("M268 failed: atomic AUTOPILOT write reservation.");
        llm_auto_outer_end("test-failed");
        return EXIT_FAILURE;
    }
    llm_auto_outer_end("reservation-test");
    llm_auto_reset();

    llm_auto_outer_begin();

    if (!llm_m268_outer_active() ||
        !llm_auto_take_turns(3U) ||
        llm_auto_take_turns(2U) ||
        llm_m268_turn_count() != 3U ||
        !llm_auto_take_writes(2U) ||
        llm_auto_take_writes(1U) ||
        llm_m268_write_count() != 2U ||
        !llm_auto_budget_exhausted()) {
        (void)puts("M268 failed: cumulative AUTOPILOT limits.");
        llm_auto_outer_end("test-failed");
        return EXIT_FAILURE;
    }

    llm_auto_outer_end("retry-budget-exhausted");

    if (llm_m268_outer_active() ||
        !llm_auto_status_text(output, sizeof(output)) ||
        !contains(output, "Turns:         3") ||
        !contains(output, "Write actions: 2") ||
        !contains(output, "Stop reason:   retry-budget-exhausted")) {
        (void)puts("M268 failed: terminal autonomous status.");
        return EXIT_FAILURE;
    }

    if (!write_text(
            M268_TEST_PLAN,
            "operation_count=1\n"
            "BEGIN_OPERATION\n"
            "type=replace_block\n"
            "path=SRC/M268_FIXTURE.C\n"
            "BEGIN_OLD_TEXT\n"
            "prefix\n"
            "\\END_OLD_TEXT\n"
            "KEEP_TOKEN\n"
            "END_OLD_TEXT\n"
            "BEGIN_NEW_TEXT\n"
            "prefix\n"
            "END_NEW_TEXT\n"
            "END_OPERATION\n")) {
        (void)puts("M268 failed: goal-guard fixture create.");
        return EXIT_FAILURE;
    }

    reason[0] = '\0';
    if (llm_m268_guard_test("repair KEEP_TOKEN", reason, sizeof(reason)) ||
        !contains(reason, "KEEP_TOKEN")) {
        (void)puts("M268 failed: current-format goal guard rejection.");
        remove_all(M268_TEST_PLAN);
        return EXIT_FAILURE;
    }

    reason[0] = '\0';
    if (!llm_m268_guard_test("remove KEEP_TOKEN", reason, sizeof(reason))) {
        (void)puts("M268 failed: explicit destructive goal guard intent.");
        remove_all(M268_TEST_PLAN);
        return EXIT_FAILURE;
    }

    remove_all(M268_TEST_PLAN);

    /* Exercise the production terminal finalizer and review writer for every
     * required terminal state with deterministic local evidence.
     */
    remove_all(M268_TEST_LOG);
    remove_all(M268_REVIEW_FILE);
    remove_all(M268_HISTORY_FILE);
    remove_all(M268_TRACE_FILE);
    remove_all(LLM_BUILD_LOG_FILE);

    if (!write_text(
            M268_TEST_LOG,
            "2026-08-22T22:00:00 workflow=AGENT/REPAIR "
            "event=repair_attempt attempt=1 plan=12345678 "
            "build=1 rollback=0 outcome=committed\n") ||
        !write_text(LLM_BUILD_LOG_FILE, "M268_BUILD_MARKER\n") ||
        !write_text(M268_TRACE_FILE, "M268_PLAN_MARKER\n")) {
        (void)puts("M268 failed: terminal review fixtures.");
        return EXIT_FAILURE;
    }

    llm_test_set_log_path(M268_TEST_LOG);
    llm_test_git_data(
        " M SRC/M268_FIXTURE.C\n",
        "diff --git a/SRC/M268_FIXTURE.C b/SRC/M268_FIXTURE.C\n"
        "+M268_DIFF_MARKER\n"
    );
    dcl_state.project_root = ".";

    if (!terminal_review_ok(&dcl_state, 1U, "CLEAN_BUILD") ||
        !terminal_review_ok(
            &dcl_state, 2U, "RETRY_BUDGET_EXHAUSTED") ||
        !terminal_review_ok(&dcl_state, 3U, "GOAL_GUARD_REJECTED")) {
        (void)puts("M268 failed: terminal state/review artifact coverage.");
        llm_test_git_data(NULL, NULL);
        llm_test_set_log_path(NULL);
        return EXIT_FAILURE;
    }

    llm_test_git_data(NULL, NULL);
    llm_test_set_log_path(NULL);
    remove_all(M268_TEST_LOG);
    remove_all(M268_REVIEW_FILE);
    remove_all(M268_HISTORY_FILE);
    remove_all(M268_TRACE_FILE);
    remove_all(LLM_BUILD_LOG_FILE);

    if (!llm_set_approval("full") ||
        strcmp(llm_approval_name(), "full") != 0 ||
        !llm_set_approval("autopilot") ||
        strcmp(llm_approval_name(), "autopilot") != 0 ||
        !llm_m268_policy_floor()) {
        (void)puts("M268 failed: FULL/AUTOPILOT policy separation.");
        return EXIT_FAILURE;
    }

    llm_reset_approval();
    llm_auto_test_limits(0U, 0U);
    llm_auto_reset();

    (void)puts(
        "M268 AUTOPILOT policy, FULL gates, limits, terminals, and review test passed."
    );
    return EXIT_SUCCESS;
}
