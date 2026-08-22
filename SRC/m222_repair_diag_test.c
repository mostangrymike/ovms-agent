#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "llm_internal.h"

#define TEST_LOG "M222_REPAIR_DIAG.LOG"

/*
 * Link-only stubs required by LLM_PLAN.OBJ.
 * This deterministic diagnostics-bundle test never uses interactive input.
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

static int seed_history(void)
{
    FILE *file;

    file = fopen(TEST_LOG, "w");
    if (file == NULL) {
        return 0;
    }

    if (fputs(
            "2026-08-07T13:00:00 workflow=AGENT event=start status=1\n"
            "2026-08-07T13:01:00 workflow=AGENT/REPAIR event=repair_attempt "
            "attempt=1 plan=22200001 build=2 rollback=2 outcome=rolled_back\n"
            "2026-08-07T13:02:00 workflow=AGENT/REPAIR event=repair_attempt "
            "attempt=2 plan=22200002 build=1 rollback=0 outcome=committed\n"
            "2026-08-07T14:00:00 workflow=AGENT/REPAIR event=repair_attempt "
            "attempt=1 plan=22200003 build=1 rollback=0 outcome=committed\n"
            "2026-08-07T15:00:00 workflow=AGENT/REPAIR event=repair_attempt "
            "attempt=1 plan=22200004 build=2 rollback=2 outcome=rolled_back\n"
            "2026-08-07T15:01:00 workflow=AGENT/REPAIR event=repair_attempt "
            "attempt=2 plan=22200005 build=2 rollback=2 outcome=rolled_back\n"
            "2026-08-07T15:02:00 workflow=AGENT/REPAIR event=repair_attempt "
            "attempt=x plan=1234 build=2 rollback=2 outcome=bogus\n",
            file) == EOF) {
        (void)fclose(file);
        return 0;
    }

    return fclose(file) == 0;
}

int main(void)
{
    char report[8192];

    cleanup();
    llm_test_set_log_path(TEST_LOG);

    if (!seed_history()) {
        (void)puts("M222 failed: unable to seed history.");
        cleanup();
        return EXIT_FAILURE;
    }

    if (!llm_repair_diag_text(
            report,
            sizeof(report)) ||
        strstr(report, "OVMS Agent repair history diagnostics") == NULL ||
        strstr(report, "Repair records:      5") == NULL ||
        strstr(report, "Repair runs:         3") == NULL ||
        strstr(report, "Malformed records:   1") == NULL ||
        strstr(report, "Integrity:           FAIL") == NULL) {
        (void)puts("M222 failed: combined diagnostics are incorrect.");
        cleanup();
        return EXIT_FAILURE;
    }

    if (!llm_repair_count_text(
            report,
            sizeof(report)) ||
        strstr(report, "Repair records: 5") == NULL ||
        strstr(report, "Repair runs:    3") == NULL) {
        (void)puts("M222 failed: concise counts are incorrect.");
        cleanup();
        return EXIT_FAILURE;
    }

    if (!llm_repair_latest_text(
            report,
            sizeof(report)) ||
        strstr(report, "OVMS Agent latest repair run") == NULL ||
        strstr(report, "Started: 2026-08-07T15:00:00") == NULL ||
        strstr(report, "plan 22200004") == NULL ||
        strstr(report, "plan 22200005") == NULL ||
        strstr(report, "Final outcome: rolled_back") == NULL) {
        (void)puts("M222 failed: latest repair run is incorrect.");
        cleanup();
        return EXIT_FAILURE;
    }

    if (!llm_repair_oldest_text(
            report,
            sizeof(report)) ||
        strstr(report, "OVMS Agent oldest repair run") == NULL ||
        strstr(report, "Started: 2026-08-07T13:01:00") == NULL ||
        strstr(report, "plan 22200001") == NULL ||
        strstr(report, "plan 22200002") == NULL ||
        strstr(report, "Final outcome: committed") == NULL) {
        (void)puts("M222 failed: oldest repair run is incorrect.");
        cleanup();
        return EXIT_FAILURE;
    }

    cleanup();
    (void)puts("Repair diagnostics bundle test passed.");
    return EXIT_SUCCESS;
}
