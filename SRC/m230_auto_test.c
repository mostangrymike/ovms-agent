#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "openai_internal.h"

#define TEST_TX "M230_TRANSCRIPT.DAT"

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
    openai_auto_test_limits(0U, 0U);
    openai_auto_reset();
    openai_test_tx_path(NULL);
    remove_all(TEST_TX);
}

int main(void)
{
    char output[16384];

    cleanup();
    openai_test_tx_path(TEST_TX);
    openai_auto_test_limits(5U, 2U);

    if (openai_auto_turn_limit(OPENAI_WORKFLOW_AGENT) != 5U ||
        openai_auto_turn_limit(OPENAI_WORKFLOW_PLAN) !=
            OPENAI_PLAN_MAX_TURNS) {
        (void)puts("M230 failed: autonomous turn limits.");
        cleanup();
        return EXIT_FAILURE;
    }

    openai_auto_begin(OPENAI_WORKFLOW_WRITE);
    openai_auto_note_turn();
    openai_auto_note_turn();
    openai_auto_note_tool();

    if (!openai_auto_allow_write() ||
        !openai_auto_allow_write() ||
        openai_auto_allow_write()) {
        (void)puts("M230 failed: bounded write accounting.");
        cleanup();
        return EXIT_FAILURE;
    }

    openai_auto_finish("turn-limit");

    if (!openai_auto_status_text(output, sizeof(output)) ||
        strstr(output, "Turns:         2") == NULL ||
        strstr(output, "Tool calls:    1") == NULL ||
        strstr(output, "Write actions: 2") == NULL ||
        strstr(output, "Stop reason:   turn-limit") == NULL) {
        (void)puts("M230 failed: autonomous status.");
        cleanup();
        return EXIT_FAILURE;
    }

    if (!openai_auto_limits_text(output, sizeof(output)) ||
        strstr(output, "Model/tool turns: 5") == NULL ||
        strstr(output, "Write actions:    2") == NULL) {
        (void)puts("M230 failed: autonomous limits display.");
        cleanup();
        return EXIT_FAILURE;
    }

    openai_tx_model_call(
        "read_file",
        "{\"path\":\"SRC/MAIN.C\"}"
    );
    openai_tx_model_result(
        "read_file",
        "ok",
        "sample tool result"
    );
    openai_tx_loop_event("agent", "final");

    if (!openai_tool_hist_text(output, sizeof(output)) ||
        strstr(output, "read_file") == NULL ||
        strstr(output, "requested") == NULL ||
        strstr(output, "ok") == NULL) {
        (void)puts("M230 failed: model tool transcript.");
        cleanup();
        return EXIT_FAILURE;
    }

    if (!openai_tool_last_text(output, sizeof(output)) ||
        strstr(output, "Tool:     read_file") == NULL ||
        strstr(output, "Status:   ok") == NULL ||
        strstr(output, "sample tool result") == NULL) {
        (void)puts("M230 failed: last model tool result.");
        cleanup();
        return EXIT_FAILURE;
    }

    if (!openai_parity_text(output, sizeof(output)) ||
        strstr(output, "Autonomous tool loop:  available") == NULL ||
        strstr(output, "Tool-result feedback:  available") == NULL ||
        strstr(output, "Bounded multi-patch:   available") == NULL) {
        (void)puts("M230 failed: parity report.");
        cleanup();
        return EXIT_FAILURE;
    }

    openai_auto_reset();

    if (!openai_auto_status_text(output, sizeof(output)) ||
        strstr(output, "Stop reason:   none") == NULL) {
        (void)puts("M230 failed: autonomous reset.");
        cleanup();
        return EXIT_FAILURE;
    }

    cleanup();
    (void)puts("Autonomous tool-loop parity bundle test passed.");
    return EXIT_SUCCESS;
}
