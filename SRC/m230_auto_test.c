#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "llm_internal.h"
#include "LLM_AUTO.H"
#include "LLM_RATE_LIMIT.H"
#include "rms_write.h"

#define TEST_TX "M230_TRANSCRIPT.DAT"
#define TEST_ROLLBACK "M230_ROLLBACK.TXT"

const char *command_prov_test_json(const char *json,
                                   int transported,
                                   int *responsive);

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
    llm_auto_test_limits(0U, 0U);
    llm_auto_reset();
    llm_test_tx_path(NULL);
    rms_run_commit();
    remove_all(TEST_TX);
    remove_all(TEST_ROLLBACK);
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

static int file_contains(const char *path, const char *text)
{
    FILE *file;
    char buffer[256];
    int found;

    file = fopen(path, "r");
    if (file == NULL) {
        return 0;
    }

    found = 0;
    while (fgets(buffer, sizeof(buffer), file) != NULL) {
        if (strstr(buffer, text) != NULL) {
            found = 1;
            break;
        }
    }

    (void)fclose(file);
    return found;
}

static int test_rate_limit_parser(void)
{
    static const char groq_error[] =
        "{\"error\":{\"message\":\"Rate limit reached for model. "
        "Limit 8000, Used 5020, Requested 4361. "
        "Please try again in 10.3575s.\","
        "\"type\":\"tokens\",\"code\":\"rate_limit_exceeded\"}}";
    static const char short_error[] =
        "{\"error\":{\"message\":\"Rate limit reached. "
        "Please try again in 2.595s.\","
        "\"code\":\"rate_limit_exceeded\"}}";
    static const char auth_error[] =
        "{\"error\":{\"message\":\"Invalid API key\","
        "\"code\":\"invalid_api_key\"}}";
    static const char no_delay[] =
        "{\"error\":{\"message\":\"Rate limit reached.\","
        "\"code\":\"rate_limit_exceeded\"}}";
    static const char long_delay[] =
        "{\"error\":{\"message\":\"Rate limit reached. "
        "Please try again in 60.1s.\","
        "\"code\":\"rate_limit_exceeded\"}}";
    static const char normal_output[] =
        "{\"output\":[{\"content\":[{\"text\":"
        "\"Please try again in 10.3s if a rate limit occurs.\"}]}]}";
    unsigned int wait_seconds;

    wait_seconds = 0U;
    if (!llm_rate_limit_delay(groq_error, &wait_seconds) ||
        wait_seconds != 11U) {
        return 0;
    }

    wait_seconds = 0U;
    if (!llm_rate_limit_delay(short_error, &wait_seconds) ||
        wait_seconds != 3U) {
        return 0;
    }

    return !llm_rate_limit_delay(auth_error, &wait_seconds) &&
           !llm_rate_limit_delay(no_delay, &wait_seconds) &&
           !llm_rate_limit_delay(long_delay, &wait_seconds) &&
           !llm_rate_limit_delay(normal_output, &wait_seconds);
}

static int test_provider_probe_parser(void)
{
    static const char ok_response[] =
        "{\"status\":\"completed\",\"output\":[{"
        "\"type\":\"message\",\"content\":[{"
        "\"type\":\"output_text\",\"text\":\"OK\"}]}]}";
    static const char api_error[] =
        "{\"error\":{\"message\":\"Invalid API key\"}}";
    static const char empty_response[] =
        "{\"status\":\"completed\",\"output\":[],"
        "\"usage\":{\"output_tokens\":0}}";
    static const char invalid_response[] =
        "{\"status\":\"completed\",\"output\":[{}]}";
    const char *status;
    int responsive;

    responsive = 0;
    status = command_prov_test_json(ok_response, 1, &responsive);
    if (status == NULL || strcmp(status, "OK") != 0 || !responsive) {
        return 0;
    }

    responsive = 1;
    status = command_prov_test_json(api_error, 1, &responsive);
    if (status == NULL || strcmp(status, "API ERROR") != 0 || responsive) {
        return 0;
    }

    responsive = 1;
    status = command_prov_test_json(empty_response, 1, &responsive);
    if (status == NULL || strcmp(status, "EMPTY") != 0 || responsive) {
        return 0;
    }

    responsive = 1;
    status = command_prov_test_json(invalid_response, 1, &responsive);
    if (status == NULL || strcmp(status, "INVALID") != 0 || responsive) {
        return 0;
    }

    responsive = 1;
    status = command_prov_test_json(NULL, 0, &responsive);
    return status != NULL &&
           strcmp(status, "TRANSPORT") == 0 &&
           !responsive;
}

int main(void)
{
    char output[16384];
    unsigned int index;

    cleanup();
    llm_test_tx_path(TEST_TX);
    llm_auto_test_limits(5U, 2U);

    if (llm_auto_turn_limit(LLM_WORKFLOW_AGENT) != 5U ||
        llm_auto_turn_limit(LLM_WORKFLOW_PLAN) !=
            LLM_PLAN_MAX_TURNS) {
        (void)puts("M230 failed: autonomous turn limits.");
        cleanup();
        return EXIT_FAILURE;
    }

    if (!test_rate_limit_parser()) {
        (void)puts("M230 failed: bounded provider rate-limit parser.");
        cleanup();
        return EXIT_FAILURE;
    }

    if (!test_provider_probe_parser()) {
        (void)puts("M292 failed: provider health response classification.");
        cleanup();
        return EXIT_FAILURE;
    }

    llm_auto_begin(LLM_WORKFLOW_WRITE);
    llm_auto_note_turn();
    llm_auto_note_turn();
    llm_auto_note_tool();

    if (!llm_auto_allow_write() ||
        !llm_auto_allow_write() ||
        llm_auto_allow_write()) {
        (void)puts("M230 failed: bounded write accounting.");
        cleanup();
        return EXIT_FAILURE;
    }

    llm_auto_finish("turn-limit");

    if (!llm_auto_status_text(output, sizeof(output)) ||
        strstr(output, "Turns:         2") == NULL ||
        strstr(output, "Tool calls:    1") == NULL ||
        strstr(output, "Write actions: 2") == NULL ||
        strstr(output, "Stop reason:   turn-limit") == NULL) {
        (void)puts("M230 failed: autonomous status.");
        cleanup();
        return EXIT_FAILURE;
    }

    if (!llm_auto_limits_text(output, sizeof(output)) ||
        strstr(output, "Model/tool turns: 5") == NULL ||
        strstr(output, "Write actions:    2") == NULL) {
        (void)puts("M230 failed: autonomous limits display.");
        cleanup();
        return EXIT_FAILURE;
    }

    llm_auto_begin(LLM_WORKFLOW_AGENT);
    if (llm_auto_final_has_evidence(LLM_WORKFLOW_AGENT)) {
        (void)puts("M230 failed: zero-tool final evidence guard.");
        cleanup();
        return EXIT_FAILURE;
    }
    llm_auto_note_tool();
    if (!llm_auto_final_has_evidence(LLM_WORKFLOW_AGENT)) {
        (void)puts("M230 failed: tool evidence final allowance.");
        cleanup();
        return EXIT_FAILURE;
    }
    llm_auto_finish("final");

    llm_auto_begin(LLM_WORKFLOW_PLAN);
    if (!llm_auto_final_has_evidence(LLM_WORKFLOW_PLAN)) {
        (void)puts("M230 failed: plan final evidence exemption.");
        cleanup();
        return EXIT_FAILURE;
    }
    llm_auto_finish("final");

    if (!write_text(TEST_ROLLBACK, "before\n")) {
        (void)puts("M230 failed: rollback fixture create.");
        cleanup();
        return EXIT_FAILURE;
    }

    llm_auto_test_limits(2U, 2U);
    llm_auto_begin(LLM_WORKFLOW_WRITE);

    if (!llm_auto_allow_write() ||
        !rms_replace_text_file(TEST_ROLLBACK, "after\n")) {
        (void)puts("M230 failed: rollback fixture write.");
        cleanup();
        return EXIT_FAILURE;
    }

    for (index = 0U; index < 2U; ++index) {
        llm_auto_note_turn();
    }

    if (!llm_auto_partial_limit()) {
        (void)puts("M230 failed: partial write limit detection.");
        cleanup();
        return EXIT_FAILURE;
    }

    llm_auto_finish("turn-limit");

    if (!file_contains(TEST_ROLLBACK, "before") ||
        file_contains(TEST_ROLLBACK, "after")) {
        (void)puts("M230 failed: incomplete write rollback.");
        cleanup();
        return EXIT_FAILURE;
    }

    if (!write_text(TEST_ROLLBACK, "bounded-before\n")) {
        (void)puts("M230 failed: bounded-write fixture create.");
        cleanup();
        return EXIT_FAILURE;
    }

    llm_auto_test_limits(3U, 3U);
    llm_auto_begin(LLM_WORKFLOW_WRITE);

    if (!llm_auto_allow_write() ||
        !rms_replace_text_file(TEST_ROLLBACK, "bounded-kept\n") ||
        llm_auto_allow_write()) {
        (void)puts("M230 failed: second applied write guard.");
        cleanup();
        return EXIT_FAILURE;
    }

    llm_auto_finish("turn-limit");

    if (!file_contains(TEST_ROLLBACK, "bounded-kept") ||
        file_contains(TEST_ROLLBACK, "bounded-before") ||
        !llm_auto_bounded_completed() ||
        !llm_auto_status_text(output, sizeof(output)) ||
        strstr(output, "Stop reason:   bounded-write") == NULL) {
        (void)puts("M230 failed: bounded approved write retention.");
        cleanup();
        return EXIT_FAILURE;
    }

    if (!write_text(TEST_ROLLBACK, "error-before\n")) {
        (void)puts("M230 failed: error rollback fixture create.");
        cleanup();
        return EXIT_FAILURE;
    }

    llm_auto_begin(LLM_WORKFLOW_WRITE);

    if (!llm_auto_allow_write() ||
        !rms_replace_text_file(TEST_ROLLBACK, "error-after\n") ||
        llm_auto_allow_write()) {
        (void)puts("M230 failed: error-path second write guard.");
        cleanup();
        return EXIT_FAILURE;
    }

    llm_auto_finish("error");

    if (!file_contains(TEST_ROLLBACK, "error-before") ||
        file_contains(TEST_ROLLBACK, "error-after") ||
        llm_auto_bounded_completed()) {
        (void)puts("M230 failed: blocked extra write must not suppress error rollback.");
        cleanup();
        return EXIT_FAILURE;
    }

    llm_tx_model_call(
        "read_file",
        "{\"path\":\"SRC/MAIN.C\"}"
    );
    llm_tx_model_result(
        "read_file",
        "ok",
        "sample tool result"
    );
    llm_tx_loop_event("agent", "final");

    if (!llm_tool_hist_text(output, sizeof(output)) ||
        strstr(output, "read_file") == NULL ||
        strstr(output, "requested") == NULL ||
        strstr(output, "ok") == NULL) {
        (void)puts("M230 failed: model tool transcript.");
        cleanup();
        return EXIT_FAILURE;
    }

    if (!llm_tool_last_text(output, sizeof(output)) ||
        strstr(output, "Tool:     read_file") == NULL ||
        strstr(output, "Status:   ok") == NULL ||
        strstr(output, "sample tool result") == NULL) {
        (void)puts("M230 failed: last model tool result.");
        cleanup();
        return EXIT_FAILURE;
    }

    if (!llm_parity_text(output, sizeof(output)) ||
        strstr(output, "Autonomous tool loop:  available") == NULL ||
        strstr(output, "Tool-result feedback:  available") == NULL ||
        strstr(output, "Bounded multi-patch:   available") == NULL) {
        (void)puts("M230 failed: parity report.");
        cleanup();
        return EXIT_FAILURE;
    }

    llm_auto_reset();

    if (!llm_auto_status_text(output, sizeof(output)) ||
        strstr(output, "Stop reason:   none") == NULL) {
        (void)puts("M230 failed: autonomous reset.");
        cleanup();
        return EXIT_FAILURE;
    }

    cleanup();
    (void)puts("Autonomous tool-loop parity bundle test passed.");
    return EXIT_SUCCESS;
}
