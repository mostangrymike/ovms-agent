#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "llm_internal.h"

#define TEST_LOG "M211_REPAIR_STATUS.LOG"

/*
 * Link-only stubs required by LLM_PLAN.OBJ.
 * This deterministic status test never uses interactive input.
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
    llm_test_set_log_path(NULL);
    while (remove(TEST_LOG) == 0) {
    }
}

static int contains_all(const char *text,
                        const char *a,
                        const char *b,
                        const char *c)
{
    return
        text != NULL &&
        strstr(text, a) != NULL &&
        strstr(text, b) != NULL &&
        strstr(text, c) != NULL;
}

int main(void)
{
    char summary[2048];

    cleanup();
    llm_test_set_log_path(TEST_LOG);

    /* Older run: must not appear in latest-run summary. */
    llm_log_repair_attempt(
        1U,
        0x11111111UL,
        2,
        LLM_ROLLBACK_SUCCEEDED,
        "rolled_back"
    );
    llm_log_repair_attempt(
        2U,
        0x22222222UL,
        1,
        LLM_ROLLBACK_NONE,
        "committed"
    );

    /* Latest run: two attempts ending in safe rollback. */
    llm_log_repair_attempt(
        1U,
        0xA1A1A1A1UL,
        2,
        LLM_ROLLBACK_SUCCEEDED,
        "rolled_back"
    );
    llm_log_repair_attempt(
        2U,
        0xB2B2B2B2UL,
        2,
        LLM_ROLLBACK_SUCCEEDED,
        "rolled_back"
    );

    if (!llm_repair_status_text(
            summary,
            sizeof(summary))) {
        (void)puts("M211 failed: repair status summary unavailable.");
        cleanup();
        return EXIT_FAILURE;
    }

    if (!contains_all(
            summary,
            "Attempts used: 2 of 2",
            "Attempt 1: plan A1A1A1A1",
            "Attempt 2: plan B2B2B2B2") ||
        strstr(summary, "Final outcome: rolled_back") == NULL ||
        strstr(summary, "11111111") != NULL ||
        strstr(summary, "22222222") != NULL) {
        (void)puts("M211 failed: latest repair run summary is incorrect.");
        cleanup();
        return EXIT_FAILURE;
    }

    /* New one-attempt run must replace the prior two-attempt run. */
    llm_log_repair_attempt(
        1U,
        0xC3C3C3C3UL,
        1,
        LLM_ROLLBACK_NONE,
        "committed"
    );

    if (!llm_repair_status_text(
            summary,
            sizeof(summary)) ||
        !contains_all(
            summary,
            "Attempts used: 1 of 2",
            "Attempt 1: plan C3C3C3C3",
            "Final outcome: committed") ||
        strstr(summary, "A1A1A1A1") != NULL ||
        strstr(summary, "B2B2B2B2") != NULL) {
        (void)puts("M211 failed: new repair run did not reset summary.");
        cleanup();
        return EXIT_FAILURE;
    }

    cleanup();
    (void)puts("Repair status summary test passed.");
    return EXIT_SUCCESS;
}
