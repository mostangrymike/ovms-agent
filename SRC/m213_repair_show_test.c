#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "llm_internal.h"

#define TEST_LOG "M213_REPAIR_SHOW.LOG"

/*
 * Link-only stubs required by OPENAI_PLAN.OBJ.
 * This deterministic detail test never uses interactive input.
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
    openai_test_set_log_path(NULL);
    while (remove(TEST_LOG) == 0) {
    }
}

int main(void)
{
    char detail[4096];

    cleanup();
    openai_test_set_log_path(TEST_LOG);

    /* Run 1: hash D00DFEED appears here first. */
    openai_log_repair_attempt(
        1U,
        0xD00DFEEDUL,
        2,
        OPENAI_ROLLBACK_SUCCEEDED,
        "rolled_back"
    );

    /* Run 2: unrelated committed repair. */
    openai_log_repair_attempt(
        1U,
        0x11112222UL,
        1,
        OPENAI_ROLLBACK_NONE,
        "committed"
    );

    /* Run 3: same requested hash appears again, on attempt 2. */
    openai_log_repair_attempt(
        1U,
        0x33334444UL,
        2,
        OPENAI_ROLLBACK_SUCCEEDED,
        "rolled_back"
    );
    openai_log_repair_attempt(
        2U,
        0xD00DFEEDUL,
        1,
        OPENAI_ROLLBACK_NONE,
        "committed"
    );

    if (!openai_repair_show_text(
            0xD00DFEEDUL,
            detail,
            sizeof(detail))) {
        (void)puts("M213 failed: requested repair detail unavailable.");
        cleanup();
        return EXIT_FAILURE;
    }

    if (strstr(detail, "Requested plan: D00DFEED") == NULL ||
        strstr(detail, "Persisted run: 3 of 3") == NULL ||
        strstr(detail, "Attempt 1: plan 33334444") == NULL ||
        strstr(detail, "Attempt 2: plan D00DFEED [requested]") == NULL ||
        strstr(detail, "Final outcome: committed") == NULL ||
        strstr(detail, "run: 1 of 3") != NULL) {
        (void)puts("M213 failed: repair detail did not select newest run.");
        cleanup();
        return EXIT_FAILURE;
    }

    if (openai_repair_show_text(
            0xABCDEF01UL,
            detail,
            sizeof(detail))) {
        (void)puts("M213 failed: missing plan hash unexpectedly matched.");
        cleanup();
        return EXIT_FAILURE;
    }

    cleanup();
    (void)puts("Repair plan drill-down test passed.");
    return EXIT_SUCCESS;
}
