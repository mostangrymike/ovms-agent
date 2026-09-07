#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "llm_internal.h"
#include "LLM_CONTEXT.H"
#include "LLM_AUTO.H"

#define TEST_SESS "M231_SESSIONS.DAT"
#define TEST_CUR  "M231_SESSION.CUR"
#define TEST_TX   "M231_TRANSCRIPT.DAT"

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
    llm_test_reset_approval();
    llm_auto_reset();
    remove_all(TEST_SESS);
    remove_all(TEST_CUR);
    remove_all(TEST_TX);
}

int main(void)
{
    char parent[9];
    char forked[9];
    char text_only[9];
    char args[128];
    char output[16384];

    cleanup();

    llm_test_session_paths(TEST_SESS, TEST_CUR);
    llm_test_tx_path(TEST_TX);

    if (!llm_session_new("parent session", parent) ||
        !llm_session_note_goal("original parser goal")) {
        (void)puts("M231 failed: parent session setup.");
        cleanup();
        return EXIT_FAILURE;
    }

    llm_tx_model_call("read_file", "SRC/PARSER.C");
    llm_tx_model_result(
        "read_file", "ok",
        "TOOL RESULT ----------- tool: read_file status: ok "
        "output: parser uses legacy token scan"
    );

    llm_auto_begin(LLM_WORKFLOW_AGENT);
    if (!llm_auto_final_has_evidence(LLM_WORKFLOW_AGENT)) {
        (void)puts("M304 failed: current-session normalized evidence.");
        cleanup();
        return EXIT_FAILURE;
    }
    llm_auto_finish("final");

    llm_auto_begin(LLM_WORKFLOW_WRITE);
    if (llm_auto_final_has_evidence(LLM_WORKFLOW_WRITE)) {
        (void)puts("M304 failed: WRITE reused prior session evidence.");
        cleanup();
        return EXIT_FAILURE;
    }
    llm_auto_finish("final");

    (void)snprintf(
        args, sizeof(args), "%s child session", parent
    );

    if (!llm_session_fork(args, forked)) {
        (void)puts("M231 failed: fork setup.");
        cleanup();
        return EXIT_FAILURE;
    }

    if (!llm_context_build(
            "continue fixing parser", output, sizeof(output)) ||
        strstr(output, "Persistent OVMS Agent session context") == NULL ||
        strstr(output, "CURRENT REQUEST") == NULL ||
        strstr(output, "continue fixing parser") == NULL ||
        strstr(output, "PARENT SESSION RECENT TRANSCRIPT") == NULL ||
        strstr(output, "PARENT SESSION PRIORITIZED TOOL EVIDENCE") == NULL ||
        strstr(output, "parser uses legacy token scan") == NULL ||
        strstr(output, "Original goal: original parser goal") == NULL) {
        (void)puts("M231 failed: restored fork context.");
        cleanup();
        return EXIT_FAILURE;
    }

    llm_auto_begin(LLM_WORKFLOW_AGENT);
    if (!llm_auto_final_has_evidence(LLM_WORKFLOW_AGENT)) {
        (void)puts("M304 failed: parent normalized evidence.");
        cleanup();
        return EXIT_FAILURE;
    }
    llm_auto_finish("final");

    if (!llm_context_current(output, sizeof(output)) ||
        strstr(output, forked) == NULL ||
        strstr(output, "Recent transcript:") == NULL) {
        (void)puts("M231 failed: current context inspection.");
        cleanup();
        return EXIT_FAILURE;
    }

    if (!llm_parity_text(output, sizeof(output)) ||
        strstr(output, "Context restoration:  available") == NULL ||
        strstr(output, "Fork ancestry context: available") == NULL) {
        (void)puts("M231 failed: parity status.");
        cleanup();
        return EXIT_FAILURE;
    }

    if (!llm_session_new("transcript only", text_only)) {
        (void)puts("M304 failed: transcript-only session setup.");
        cleanup();
        return EXIT_FAILURE;
    }
    llm_tx_loop_event("agent", "final");

    llm_auto_begin(LLM_WORKFLOW_AGENT);
    if (llm_auto_final_has_evidence(LLM_WORKFLOW_AGENT)) {
        (void)puts("M304 failed: transcript-only evidence accepted.");
        cleanup();
        return EXIT_FAILURE;
    }
    llm_auto_finish("error");

    cleanup();
    (void)puts("Persistent context restoration bundle test passed.");
    (void)puts("M304 persisted-session evidence regression passed.");
    return EXIT_SUCCESS;
}
