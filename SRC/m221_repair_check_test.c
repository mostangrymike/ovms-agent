#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "llm_internal.h"

#define TEST_LOG "M221_REPAIR_CHECK.LOG"

/*
 * Link-only stubs required by OPENAI_PLAN.OBJ.
 * This deterministic integrity test never uses interactive input.
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

static int write_text(const char *text)
{
    FILE *file;

    file = fopen(TEST_LOG, "w");
    if (file == NULL) {
        return 0;
    }

    if (fputs(text, file) == EOF) {
        (void)fclose(file);
        return 0;
    }

    return fclose(file) == 0;
}

int main(void)
{
    char report[2048];

    cleanup();
    openai_test_set_log_path(TEST_LOG);

    if (!write_text(
            "2026-08-07T10:00:00 workflow=AGENT event=start status=1\n"
            "2026-08-07T10:01:00 workflow=AGENT/REPAIR event=repair_attempt "
            "attempt=1 plan=22100001 build=2 rollback=2 outcome=rolled_back\n"
            "2026-08-07T10:02:00 workflow=AGENT/REPAIR event=repair_attempt "
            "attempt=2 plan=22100002 build=1 rollback=0 outcome=committed\n"
            "2026-08-07T11:00:00 workflow=AGENT/REPAIR event=repair_attempt "
            "attempt=1 plan=22100003 build=1 rollback=0 outcome=committed\n")) {
        (void)puts("M221 failed: unable to seed valid history.");
        cleanup();
        return EXIT_FAILURE;
    }

    if (!openai_repair_check_text(
            report,
            sizeof(report)) ||
        strstr(report, "Records scanned:     3") == NULL ||
        strstr(report, "Runs scanned:        2") == NULL ||
        strstr(report, "Malformed records:   0") == NULL ||
        strstr(report, "Sequence errors:     0") == NULL ||
        strstr(report, "Outcome errors:      0") == NULL ||
        strstr(report, "Plan hash errors:    0") == NULL ||
        strstr(report, "Integrity:           PASS") == NULL) {
        (void)puts("M221 failed: valid history did not pass.");
        cleanup();
        return EXIT_FAILURE;
    }

    if (!write_text(
            "2026-08-07T12:00:00 workflow=AGENT/REPAIR event=repair_attempt "
            "attempt=2 plan=22100004 build=2 rollback=2 outcome=rolled_back\n"
            "2026-08-07T12:01:00 workflow=AGENT/REPAIR event=repair_attempt "
            "attempt=1 plan=1234 build=2 rollback=2 outcome=rolled_back\n"
            "2026-08-07T12:02:00 workflow=AGENT/REPAIR event=repair_attempt "
            "attempt=2 plan=22100005 build=2 rollback=2 outcome=bogus\n"
            "2026-08-07T12:03:00 workflow=AGENT/REPAIR event=repair_attempt "
            "attempt=2 plan=22100006 build=2 rollback=2 outcome=rolled_back\n"
            "2026-08-07T12:04:00 workflow=AGENT/REPAIR event=repair_attempt "
            "attempt=3 plan=22100007 build=2 rollback=2 outcome=rolled_back\n"
            "2026-08-07T12:05:00 workflow=AGENT/REPAIR event=repair_attempt "
            "attempt=x plan=22100008 build=2 rollback=2 outcome=rolled_back\n")) {
        (void)puts("M221 failed: unable to seed invalid history.");
        cleanup();
        return EXIT_FAILURE;
    }

    if (!openai_repair_check_text(
            report,
            sizeof(report)) ||
        strstr(report, "Records scanned:     6") == NULL ||
        strstr(report, "Runs scanned:        1") == NULL ||
        strstr(report, "Malformed records:   1") == NULL ||
        strstr(report, "Sequence errors:     3") == NULL ||
        strstr(report, "Outcome errors:      1") == NULL ||
        strstr(report, "Plan hash errors:    1") == NULL ||
        strstr(report, "Integrity:           FAIL") == NULL) {
        (void)puts("M221 failed: invalid history counts are incorrect.");
        cleanup();
        return EXIT_FAILURE;
    }

    cleanup();
    (void)puts("Repair history integrity test passed.");
    return EXIT_SUCCESS;
}
