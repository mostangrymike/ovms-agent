#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "openai_internal.h"

#define TEST_LOG "M220_REPAIR_INFO.LOG"

/*
 * Link-only stubs required by OPENAI_PLAN.OBJ.
 * This deterministic history-info test never uses interactive input.
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
    openai_test_set_history_limit(0U);
    openai_test_set_log_path(NULL);

    while (remove(TEST_LOG) == 0) {
    }
}

static int write_seed(void)
{
    FILE *file;

    file = fopen(TEST_LOG, "w");
    if (file == NULL) {
        return 0;
    }

    if (fputs(
            "2026-08-07T10:00:00 workflow=AGENT event=start status=1\n"
            "2026-08-07T10:01:00 workflow=AGENT/REPAIR event=repair_attempt "
            "attempt=1 plan=22000001 build=2 rollback=2 outcome=rolled_back\n"
            "malformed workflow=AGENT/REPAIR event=repair_attempt junk\n"
            "2026-08-07T10:02:00 workflow=AGENT/REPAIR event=repair_attempt "
            "attempt=2 plan=22000002 build=1 rollback=0 outcome=committed\n"
            "2026-08-07T11:00:00 workflow=AGENT/REPAIR event=repair_attempt "
            "attempt=1 plan=22000003 build=1 rollback=0 outcome=committed\n",
            file) == EOF) {
        (void)fclose(file);
        return 0;
    }

    return fclose(file) == 0;
}

int main(void)
{
    char information[2048];

    cleanup();
    openai_test_set_log_path(TEST_LOG);
    openai_test_set_history_limit(7U);

    if (!openai_repair_info_text(
            information,
            sizeof(information)) ||
        strstr(information, "Activity log:        M220_REPAIR_INFO.LOG") == NULL ||
        strstr(information, "Repair records:      0") == NULL ||
        strstr(information, "Repair runs:         0") == NULL ||
        strstr(information, "Oldest repair:       none") == NULL ||
        strstr(information, "Newest repair:       none") == NULL ||
        strstr(information, "History window:      7") == NULL ||
        strstr(information, "History available:   no") == NULL) {
        (void)puts("M220 failed: empty history metadata is incorrect.");
        cleanup();
        return EXIT_FAILURE;
    }

    if (!write_seed()) {
        (void)puts("M220 failed: unable to seed repair history.");
        cleanup();
        return EXIT_FAILURE;
    }

    if (!openai_repair_info_text(
            information,
            sizeof(information)) ||
        strstr(information, "Activity log:        M220_REPAIR_INFO.LOG") == NULL ||
        strstr(information, "Repair records:      3") == NULL ||
        strstr(information, "Repair runs:         2") == NULL ||
        strstr(information, "Oldest repair:       2026-08-07T10:01:00") == NULL ||
        strstr(information, "Newest repair:       2026-08-07T11:00:00") == NULL ||
        strstr(information, "History window:      7") == NULL ||
        strstr(information, "History available:   yes") == NULL) {
        (void)puts("M220 failed: populated history metadata is incorrect.");
        cleanup();
        return EXIT_FAILURE;
    }

    cleanup();
    (void)puts("Repair history information test passed.");
    return EXIT_SUCCESS;
}
