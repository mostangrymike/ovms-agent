#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "llm_internal.h"
#include "command_internal.h"

#define TEST_SESS "M229_SESSIONS.DAT"
#define TEST_CUR  "M229_SESSION.CUR"
#define TEST_TX   "M229_TRANSCRIPT.DAT"
#define TEST_OUT  "M229_EXPORT.TXT"
#define TEST_LOG  "M254_DCL_ACTIVITY.LOG"

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

static void remove_all(const char *path)
{
    while (remove(path) == 0) {
    }
}

static void cleanup(void)
{
    llm_test_session_paths(NULL, NULL);
    llm_test_tx_path(NULL);
    llm_test_set_log_path(NULL);
    llm_test_reset_approval();
    remove_all(TEST_SESS);
    remove_all(TEST_CUR);
    remove_all(TEST_TX);
    remove_all(TEST_OUT);
    remove_all(TEST_LOG);
}

int main(void)
{
    agent_state state;
    char session_id[9];
    char output[32768];
    char args[1024];
    unsigned long dcl_status;

    cleanup();

    (void)memset(&state, 0, sizeof(state));
    state.project_root = ".";
    state.api_key_defined = 1;
    state.write_enabled = 0;
    state.dcl_enabled = 0;

    llm_test_session_paths(TEST_SESS, TEST_CUR);
    llm_test_tx_path(TEST_TX);
    llm_test_set_log_path(TEST_LOG);

    if (!llm_session_new("m229 session", session_id)) {
        (void)puts("M229 failed: create session.");
        cleanup();
        return EXIT_FAILURE;
    }

    if (!llm_tool_run(&state, "READ NO_SUCH_M229_FILE.TXT") ||
        !llm_tool_last_text(output, sizeof(output)) ||
        strstr(output, "Tool:     READ") == NULL ||
        strstr(output, "Status:   dispatched") == NULL ||
        strstr(output, session_id) == NULL) {
        (void)puts("M229 failed: read tool transcript.");
        cleanup();
        return EXIT_FAILURE;
    }

    if (llm_tool_run(&state, "EDIT FOO.TXT 1 x") ||
        !llm_tool_last_text(output, sizeof(output)) ||
        strstr(output, "Tool:     EDIT") == NULL ||
        strstr(output, "Status:   denied") == NULL) {
        (void)puts("M229 failed: approval denial transcript.");
        cleanup();
        return EXIT_FAILURE;
    }

    if (!llm_set_approval("workspace")) {
        (void)puts("M229 failed: set workspace approval.");
        cleanup();
        return EXIT_FAILURE;
    }

    if (llm_tool_run(&state, "BUILD") ||
        !llm_tool_last_text(output, sizeof(output)) ||
        strstr(output, "Tool:     BUILD") == NULL ||
        strstr(output, "Status:   write-gate") == NULL) {
        (void)puts("M229 failed: write gate transcript.");
        cleanup();
        return EXIT_FAILURE;
    }

    if (llm_tool_run(&state, "RUN \"SHOW TIME\"") ||
        !llm_tool_last_text(output, sizeof(output)) ||
        strstr(output, "Tool:     RUN") == NULL ||
        strstr(output, "Policy:   workspace") == NULL ||
        strstr(output, "Status:   denied") == NULL) {
        (void)puts("M254 failed: workspace/full policy boundary.");
        cleanup();
        return EXIT_FAILURE;
    }

    if (command_dcl_exec(&state, "SHOW DEFAULT",
                         output, sizeof(output), &dcl_status) ||
        strstr(output, "full approval policy required") == NULL) {
        (void)puts("M254 failed: DCL workspace policy refusal.");
        cleanup();
        return EXIT_FAILURE;
    }

    if (!llm_set_approval("full")) {
        (void)puts("M254 failed: set full approval.");
        cleanup();
        return EXIT_FAILURE;
    }

    state.write_enabled = 1;
    state.dcl_enabled = 0;

    if (llm_tool_run(&state, "RUN \"SHOW TIME\"") ||
        !llm_tool_last_text(output, sizeof(output)) ||
        strstr(output, "Tool:     RUN") == NULL ||
        strstr(output, "Policy:   full") == NULL ||
        strstr(output, "Status:   dcl-gate") == NULL) {
        (void)puts("M254 failed: full policy DCL gate boundary.");
        cleanup();
        return EXIT_FAILURE;
    }

    if (command_dcl_exec(&state, "SHOW DEFAULT",
                         output, sizeof(output), &dcl_status) ||
        strstr(output, "DCL gate is disabled") == NULL) {
        (void)puts("M254 failed: approved DCL gate refusal.");
        cleanup();
        return EXIT_FAILURE;
    }

    state.dcl_enabled = 1;

    if (!command_dcl_exec(&state, "SHOW DEFAULT",
                          output, sizeof(output), &dcl_status)) {
        (void)puts("M254 failed: approved DCL execution dispatch.");
        cleanup();
        return EXIT_FAILURE;
    }
    if ((dcl_status & 1UL) == 0UL) {
        (void)printf("M254 failed: approved DCL status was %%X%08lX.\n",
                     dcl_status);
        cleanup();
        return EXIT_FAILURE;
    }
    if (output[0] == '\0') {
        (void)puts("M254 failed: approved DCL output capture empty.");
        cleanup();
        return EXIT_FAILURE;
    }
    if (!llm_tool_last_text(args, sizeof(args))) {
        (void)puts("M254 failed: approved DCL last tool transcript unavailable.");
        cleanup();
        return EXIT_FAILURE;
    }
    if (strstr(args, "Tool:     DCL") == NULL) {
        (void)puts("M254 failed: approved DCL transcript missing tool name.");
        cleanup();
        return EXIT_FAILURE;
    }
    if (strstr(args, "Policy:   full") == NULL) {
        (void)puts("M254 failed: approved DCL transcript missing full policy.");
        cleanup();
        return EXIT_FAILURE;
    }
    if (strstr(args, "Status:   success") == NULL) {
        (void)puts("M254 failed: approved DCL transcript missing success status.");
        cleanup();
        return EXIT_FAILURE;
    }

    if (command_dcl_exec(&state, "EXIT",
                         output, sizeof(output), &dcl_status)) {
        (void)puts("M254 failed: DCL wrapper control rejection.");
        cleanup();
        return EXIT_FAILURE;
    }

    if (!llm_tool_hist_text(output, sizeof(output)) ||
        strstr(output, "READ") == NULL ||
        strstr(output, "EDIT") == NULL ||
        strstr(output, "BUILD") == NULL ||
        strstr(output, "RUN") == NULL ||
        strstr(output, "DCL") == NULL) {
        (void)puts("M229 failed: tool history.");
        cleanup();
        return EXIT_FAILURE;
    }

    if (!llm_session_hist_text(session_id, output, sizeof(output)) ||
        strstr(output, "kind=tool") == NULL ||
        strstr(output, "name=READ") == NULL ||
        strstr(output, "name=EDIT") == NULL ||
        strstr(output, "name=RUN") == NULL ||
        strstr(output, "name=DCL") == NULL) {
        (void)puts("M229 failed: session transcript.");
        cleanup();
        return EXIT_FAILURE;
    }

    (void)snprintf(
        args, sizeof(args),
        "%s %s", session_id, TEST_OUT
    );

    if (!llm_session_export(args)) {
        (void)puts("M229 failed: session export.");
        cleanup();
        return EXIT_FAILURE;
    }

    if (llm_session_export(args)) {
        (void)puts("M229 failed: export overwrite protection.");
        cleanup();
        return EXIT_FAILURE;
    }

    if (!llm_parity_text(output, sizeof(output)) ||
        strstr(output, "Session transcripts:   available") == NULL ||
        strstr(output, "Direct tool runner:    available") == NULL ||
        strstr(output, "Resume/fork execution: available") == NULL) {
        (void)puts("M229 failed: parity status.");
        cleanup();
        return EXIT_FAILURE;
    }

    if (llm_tool_clear("NO") ||
        !llm_tool_clear("CLEAR TOOL HISTORY") ||
        !llm_tool_hist_text(output, sizeof(output)) ||
        strstr(output, "Entries shown: 0") == NULL) {
        (void)puts("M229 failed: transcript clear.");
        cleanup();
        return EXIT_FAILURE;
    }

    cleanup();
    (void)puts("Session transcript and tool runner bundle test passed.");
    return EXIT_SUCCESS;
}
