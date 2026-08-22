#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "llm_internal.h"

#define TEST_LOG "M217_REPAIR_WINDOW.LOG"

/*
 * Link-only stubs required by LLM_PLAN.OBJ.
 * This deterministic window test never uses interactive input.
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

static void cleanup(void)
{
    llm_test_set_history_limit(0U);
    llm_test_set_log_path(NULL);

    while (remove(TEST_LOG) == 0) {
    }
}

static void add_success(unsigned long hash)
{
    llm_log_repair_attempt(
        1U,
        hash,
        1,
        0,
        "committed"
    );
}

int main(void)
{
    char history[32768];
    char statistics[4096];
    unsigned int index;

    cleanup();
    llm_test_set_log_path(TEST_LOG);

    if (llm_test_history_limit(NULL) != 5U ||
        llm_test_history_limit("") != 5U ||
        llm_test_history_limit("0") != 5U ||
        llm_test_history_limit("abc") != 5U ||
        llm_test_history_limit("21") != 5U ||
        llm_test_history_limit("2") != 2U ||
        llm_test_history_limit("20") != 20U) {
        (void)puts("M217 failed: history-limit validation is incorrect.");
        cleanup();
        return EXIT_FAILURE;
    }

    for (index = 1U; index <= 8U; ++index) {
        add_success(0x71000000UL + (unsigned long)index);
    }

    llm_test_set_history_limit(2U);

    if (!llm_repair_history_text(
            history,
            sizeof(history)) ||
        strstr(history, "Recent runs: 2 (maximum 2)") == NULL ||
        strstr(history, "71000008") == NULL ||
        strstr(history, "71000007") == NULL ||
        strstr(history, "71000006") != NULL) {
        (void)puts("M217 failed: two-run history window is incorrect.");
        cleanup();
        return EXIT_FAILURE;
    }

    if (!llm_repair_stats_text(
            statistics,
            sizeof(statistics)) ||
        strstr(statistics, "Runs analyzed:              2") == NULL) {
        (void)puts("M217 failed: statistics did not use two-run window.");
        cleanup();
        return EXIT_FAILURE;
    }

    llm_test_set_history_limit(7U);

    if (!llm_repair_history_text(
            history,
            sizeof(history)) ||
        strstr(history, "Recent runs: 7 (maximum 7)") == NULL ||
        strstr(history, "71000008") == NULL ||
        strstr(history, "71000002") == NULL ||
        strstr(history, "71000001") != NULL) {
        (void)puts("M217 failed: seven-run history window is incorrect.");
        cleanup();
        return EXIT_FAILURE;
    }

    if (!llm_repair_stats_text(
            statistics,
            sizeof(statistics)) ||
        strstr(statistics, "Runs analyzed:              7") == NULL) {
        (void)puts("M217 failed: statistics did not use seven-run window.");
        cleanup();
        return EXIT_FAILURE;
    }

    llm_test_set_history_limit(5U);

    if (!llm_repair_history_text(
            history,
            sizeof(history)) ||
        strstr(history, "Recent runs: 5 (maximum 5)") == NULL ||
        strstr(history, "71000008") == NULL ||
        strstr(history, "71000004") == NULL ||
        strstr(history, "71000003") != NULL) {
        (void)puts("M217 failed: default five-run behavior changed.");
        cleanup();
        return EXIT_FAILURE;
    }

    cleanup();
    (void)puts("Configurable repair history window test passed.");
    return EXIT_SUCCESS;
}
