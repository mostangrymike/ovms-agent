#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "llm_internal.h"

#define TEST_LOG "M225_REPAIR_TREND.LOG"

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

static void remove_all(const char *path)
{
    while (remove(path) == 0) {
    }
}

static void cleanup(void)
{
    llm_test_set_history_limit(0U);
    llm_test_set_log_path(NULL);
    remove_all(TEST_LOG);
}

static int seed_history(void)
{
    FILE *file;

    file = fopen(TEST_LOG, "w");
    if (file == NULL) {
        return 0;
    }

    if (fputs(
            "2026-08-07T09:00:00 workflow=AGENT/REPAIR event=repair_attempt "
            "attempt=1 plan=22500001 build=2 rollback=2 outcome=rolled_back\n"
            "2026-08-07T10:00:00 workflow=AGENT/REPAIR event=repair_attempt "
            "attempt=1 plan=22500002 build=2 rollback=2 outcome=rolled_back\n"
            "2026-08-07T10:01:00 workflow=AGENT/REPAIR event=repair_attempt "
            "attempt=2 plan=22500003 build=2 rollback=2 outcome=rolled_back\n"
            "2026-08-07T11:00:00 workflow=AGENT/REPAIR event=repair_attempt "
            "attempt=1 plan=22500004 build=1 rollback=0 outcome=committed\n"
            "2026-08-07T12:00:00 workflow=AGENT/REPAIR event=repair_attempt "
            "attempt=1 plan=22500005 build=2 rollback=2 outcome=rolled_back\n"
            "2026-08-07T13:00:00 workflow=AGENT/REPAIR event=repair_attempt "
            "attempt=1 plan=22500006 build=2 rollback=2 outcome=rolled_back\n"
            "2026-08-07T13:01:00 workflow=AGENT/REPAIR event=repair_attempt "
            "attempt=2 plan=22500007 build=1 rollback=0 outcome=committed\n"
            "2026-08-07T14:00:00 workflow=AGENT/REPAIR event=repair_attempt "
            "attempt=1 plan=22500008 build=1 rollback=0 outcome=committed\n",
            file) == EOF) {
        (void)fclose(file);
        return 0;
    }

    return fclose(file) == 0;
}

int main(void)
{
    char output[8192];

    cleanup();
    llm_test_set_log_path(TEST_LOG);
    llm_test_set_history_limit(6U);

    if (!seed_history()) {
        (void)puts("M225 failed: unable to seed history.");
        cleanup();
        return EXIT_FAILURE;
    }

    if (!llm_compare_text(
            output,
            sizeof(output)) ||
        strstr(output, "Latest attempts:  1") == NULL ||
        strstr(output, "Latest outcome:   committed") == NULL ||
        strstr(output, "Previous attempts:2") == NULL ||
        strstr(output, "Previous outcome: committed") == NULL ||
        strstr(output, "Reliability:      improved") == NULL) {
        (void)puts("M225 failed: comparison output is incorrect.");
        cleanup();
        return EXIT_FAILURE;
    }

    if (!llm_streak_text(
            output,
            sizeof(output)) ||
        strstr(output, "Current outcome: committed") == NULL ||
        strstr(output, "Streak length: 2") == NULL) {
        (void)puts("M225 failed: streak output is incorrect.");
        cleanup();
        return EXIT_FAILURE;
    }

    if (!llm_recovery_text(
            output,
            sizeof(output)) ||
        strstr(output, "Two-attempt runs:   2") == NULL ||
        strstr(output, "Recovered:          1") == NULL ||
        strstr(output, "Unrecovered:        1") == NULL ||
        strstr(output, "Recovery rate:      50%") == NULL) {
        (void)puts("M225 failed: recovery output is incorrect.");
        cleanup();
        return EXIT_FAILURE;
    }

    if (!llm_trend_text(
            output,
            sizeof(output)) ||
        strstr(output, "Outcomes newest-first: C,C,R,C,R,R") == NULL ||
        strstr(output, "Newer-half success: 66% (2/3)") == NULL ||
        strstr(output, "Older-half success: 33% (1/3)") == NULL ||
        strstr(output, "Trend: improving") == NULL) {
        (void)puts("M225 failed: trend output is incorrect.");
        cleanup();
        return EXIT_FAILURE;
    }

    llm_test_set_history_limit(1U);

    if (!llm_compare_text(
            output,
            sizeof(output)) ||
        strstr(output, "Comparison: unavailable") == NULL ||
        !llm_trend_text(
            output,
            sizeof(output)) ||
        strstr(output, "Trend: unavailable") == NULL) {
        (void)puts("M225 failed: small-history handling is incorrect.");
        cleanup();
        return EXIT_FAILURE;
    }

    cleanup();
    (void)puts("Repair trend comparison bundle test passed.");
    return EXIT_SUCCESS;
}
