#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "openai_internal.h"

#define TEST_LOG "M223_REPAIR_QUERY.LOG"

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
    openai_test_set_history_limit(0U);
    openai_test_set_log_path(NULL);

    while (remove(TEST_LOG) == 0) {
    }
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
            "attempt=1 plan=AAA00001 build=1 rollback=0 outcome=committed\n"
            "2026-08-07T10:00:00 workflow=AGENT/REPAIR event=repair_attempt "
            "attempt=1 plan=BBB00001 build=2 rollback=2 outcome=rolled_back\n"
            "2026-08-07T10:01:00 workflow=AGENT/REPAIR event=repair_attempt "
            "attempt=2 plan=BBB00002 build=1 rollback=0 outcome=committed\n"
            "2026-08-07T11:00:00 workflow=AGENT/REPAIR event=repair_attempt "
            "attempt=1 plan=CCC00001 build=2 rollback=2 outcome=rolled_back\n"
            "2026-08-07T11:01:00 workflow=AGENT/REPAIR event=repair_attempt "
            "attempt=2 plan=CCC00002 build=2 rollback=2 outcome=rolled_back\n"
            "2026-08-07T12:00:00 workflow=AGENT/REPAIR event=repair_attempt "
            "attempt=1 plan=DDD00001 build=2 rollback=3 outcome=unsafe\n"
            "2026-08-07T12:30:00 workflow=AGENT/REPAIR event=repair_attempt "
            "attempt=1 plan=EEE00001 build=2 rollback=2 outcome=rolled_back\n"
            "2026-08-07T13:00:00 workflow=AGENT/REPAIR event=repair_attempt "
            "attempt=1 plan=ABCDEF01 build=1 rollback=0 outcome=committed\n"
            "2026-08-07T14:00:00 workflow=AGENT/REPAIR event=repair_attempt "
            "attempt=1 plan=ABCDEF99 build=2 rollback=2 outcome=rolled_back\n"
            "2026-08-07T14:01:00 workflow=AGENT/REPAIR event=repair_attempt "
            "attempt=2 plan=12345678 build=1 rollback=0 outcome=committed\n",
            file) == EOF) {
        (void)fclose(file);
        return 0;
    }

    return fclose(file) == 0;
}

int main(void)
{
    char report[32768];

    cleanup();
    openai_test_set_log_path(TEST_LOG);
    openai_test_set_history_limit(5U);

    if (!seed_history()) {
        (void)puts("M223 failed: unable to seed repair history.");
        cleanup();
        return EXIT_FAILURE;
    }

    if (!openai_query_outcome_text(
            "committed",
            report,
            sizeof(report)) ||
        strstr(report, "Matches: 2") == NULL ||
        strstr(report, "ABCDEF99") == NULL ||
        strstr(report, "ABCDEF01") == NULL ||
        strstr(report, "BBB00001") != NULL) {
        (void)puts("M223 failed: outcome query is incorrect.");
        cleanup();
        return EXIT_FAILURE;
    }

    if (!openai_query_attempts_text(
            "2",
            report,
            sizeof(report)) ||
        strstr(report, "Matches: 2") == NULL ||
        strstr(report, "ABCDEF99") == NULL ||
        strstr(report, "CCC00001") == NULL ||
        strstr(report, "BBB00001") != NULL) {
        (void)puts("M223 failed: attempt-count query is incorrect.");
        cleanup();
        return EXIT_FAILURE;
    }

    if (!openai_query_plan_text(
            "abcd",
            report,
            sizeof(report)) ||
        strstr(report, "Matches: 2") == NULL ||
        strstr(report, "ABCDEF99") == NULL ||
        strstr(report, "ABCDEF01") == NULL) {
        (void)puts("M223 failed: plan-prefix query is incorrect.");
        cleanup();
        return EXIT_FAILURE;
    }

    if (!openai_query_since_text(
            "2026-08-07T13:",
            report,
            sizeof(report)) ||
        strstr(report, "Matches: 2") == NULL ||
        strstr(report, "2026-08-07T14:00:00") == NULL ||
        strstr(report, "2026-08-07T13:00:00") == NULL ||
        strstr(report, "2026-08-07T12:00:00") != NULL) {
        (void)puts("M223 failed: since query is incorrect.");
        cleanup();
        return EXIT_FAILURE;
    }

    if (openai_query_outcome_text(
            "bogus",
            report,
            sizeof(report)) ||
        openai_query_attempts_text(
            "3",
            report,
            sizeof(report)) ||
        openai_query_plan_text(
            "XYZ",
            report,
            sizeof(report)) ||
        openai_query_since_text(
            "yesterday",
            report,
            sizeof(report))) {
        (void)puts("M223 failed: invalid query argument was accepted.");
        cleanup();
        return EXIT_FAILURE;
    }

    cleanup();
    (void)puts("Repair history query bundle test passed.");
    return EXIT_SUCCESS;
}
