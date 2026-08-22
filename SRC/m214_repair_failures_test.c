#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "llm_internal.h"

#define TEST_LOG "M214_REPAIR_FAILURES.LOG"

/*
 * Link-only stubs required by LLM_PLAN.OBJ.
 * This deterministic filter test never uses interactive input.
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

static void add_run(unsigned long hash,
                    int build_status,
                    int rollback,
                    const char *outcome)
{
    llm_log_repair_attempt(
        1U,
        hash,
        build_status,
        rollback,
        outcome
    );
}

int main(void)
{
    char history[8192];
    const char *newer;
    const char *older;

    cleanup();
    llm_test_set_log_path(TEST_LOG);

    /*
     * Seven chronological runs.  The M212 history window retains only
     * runs 3 through 7.  Run 2 is a failure but must not be pulled back
     * into the filtered view merely to increase the failure count.
     */
    add_run(0x10000001UL, 1, LLM_ROLLBACK_NONE, "committed");
    add_run(0x20000002UL, 2, LLM_ROLLBACK_SUCCEEDED, "rolled_back");
    add_run(0x30000003UL, 1, LLM_ROLLBACK_NONE, "committed");
    add_run(0x40000004UL, 2, LLM_ROLLBACK_SUCCEEDED, "rolled_back");
    add_run(0x50000005UL, 1, LLM_ROLLBACK_NONE, "committed");
    add_run(0x60000006UL, 2, LLM_ROLLBACK_SUCCEEDED, "unsafe");
    add_run(0x70000007UL, 2, LLM_ROLLBACK_SUCCEEDED, "rolled_back");

    if (!llm_repair_failures_text(
            history,
            sizeof(history))) {
        (void)puts("M214 failed: filtered repair history unavailable.");
        cleanup();
        return EXIT_FAILURE;
    }

    if (strstr(history, "History window: 5 recent runs") == NULL ||
        strstr(history, "Failed runs in window: 3") == NULL ||
        strstr(history, "70000007") == NULL ||
        strstr(history, "60000006") == NULL ||
        strstr(history, "40000004") == NULL ||
        strstr(history, "30000003") != NULL ||
        strstr(history, "50000005") != NULL ||
        strstr(history, "20000002") != NULL ||
        strstr(history, "10000001") != NULL) {
        (void)puts("M214 failed: failure filter or history bound is wrong.");
        cleanup();
        return EXIT_FAILURE;
    }

    newer = strstr(history, "70000007");
    older = strstr(history, "60000006");

    if (newer == NULL ||
        older == NULL ||
        newer >= older) {
        (void)puts("M214 failed: failed runs are not newest-first.");
        cleanup();
        return EXIT_FAILURE;
    }

    newer = strstr(history, "60000006");
    older = strstr(history, "40000004");

    if (newer == NULL ||
        older == NULL ||
        newer >= older) {
        (void)puts("M214 failed: failure ordering is incorrect.");
        cleanup();
        return EXIT_FAILURE;
    }

    cleanup();
    (void)puts("Filtered repair failure history test passed.");
    return EXIT_SUCCESS;
}
