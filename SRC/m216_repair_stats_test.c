#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "openai_internal.h"

#define TEST_LOG "M216_REPAIR_STATS.LOG"

/*
 * Link-only stubs required by OPENAI_PLAN.OBJ.
 * This deterministic statistics test never uses interactive input.
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

static void add_one_attempt_success(unsigned long hash)
{
    openai_log_repair_attempt(
        1U,
        hash,
        1,
        OPENAI_ROLLBACK_NONE,
        "committed"
    );
}

static void add_two_attempt_success(unsigned long first_hash,
                                    unsigned long second_hash)
{
    openai_log_repair_attempt(
        1U,
        first_hash,
        2,
        OPENAI_ROLLBACK_SUCCEEDED,
        "rolled_back"
    );
    openai_log_repair_attempt(
        2U,
        second_hash,
        1,
        OPENAI_ROLLBACK_NONE,
        "committed"
    );
}

static void add_two_attempt_failure(unsigned long first_hash,
                                    unsigned long second_hash)
{
    openai_log_repair_attempt(
        1U,
        first_hash,
        2,
        OPENAI_ROLLBACK_SUCCEEDED,
        "rolled_back"
    );
    openai_log_repair_attempt(
        2U,
        second_hash,
        2,
        OPENAI_ROLLBACK_SUCCEEDED,
        "rolled_back"
    );
}

int main(void)
{
    char statistics[2048];

    cleanup();
    openai_test_set_log_path(TEST_LOG);

    /*
     * Seven chronological runs.  Only runs 3 through 7 belong to the
     * bounded M212 history window and therefore contribute to STATS.
     *
     * Retained window:
     *   run 3: first-attempt success
     *   run 4: second-attempt recovery
     *   run 5: two-attempt failure
     *   run 6: second-attempt recovery
     *   run 7: two-attempt failure
     *
     * Expected: 5 runs, 3 committed, 2 failed, 1 first success,
     * 2 second recoveries, 2 two-attempt failures, 6 rollbacks, 60%.
     */
    add_two_attempt_failure(0x10000001UL, 0x10000002UL);
    add_one_attempt_success(0x20000001UL);

    add_one_attempt_success(0x30000001UL);
    add_two_attempt_success(0x40000001UL, 0x40000002UL);
    add_two_attempt_failure(0x50000001UL, 0x50000002UL);
    add_two_attempt_success(0x60000001UL, 0x60000002UL);
    add_two_attempt_failure(0x70000001UL, 0x70000002UL);

    if (!openai_repair_stats_text(
            statistics,
            sizeof(statistics))) {
        (void)puts("M216 failed: repair statistics unavailable.");
        cleanup();
        return EXIT_FAILURE;
    }

    if (strstr(statistics, "Runs analyzed:              5") == NULL ||
        strstr(statistics, "Committed runs:             3") == NULL ||
        strstr(statistics, "Failed/rolled-back runs:    2") == NULL ||
        strstr(statistics, "First-attempt successes:    1") == NULL ||
        strstr(statistics, "Second-attempt recoveries:  2") == NULL ||
        strstr(statistics, "Two-attempt failures:       2") == NULL ||
        strstr(statistics, "Rollback operations:        6") == NULL ||
        strstr(statistics, "Success rate:               60%") == NULL) {
        (void)puts("M216 failed: repair statistics are incorrect.");
        cleanup();
        return EXIT_FAILURE;
    }

    cleanup();
    (void)puts("Repair statistics test passed.");
    return EXIT_SUCCESS;
}
