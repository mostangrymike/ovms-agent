#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "llm_internal.h"

#define TEST_LOG "M226_REPAIR_SELECT.LOG"

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
    openai_test_set_history_limit(0U);
    openai_test_set_log_path(NULL);
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
            "attempt=1 plan=22600001 build=1 rollback=0 outcome=committed\n"
            "2026-08-07T10:00:00 workflow=AGENT/REPAIR event=repair_attempt "
            "attempt=1 plan=22600002 build=2 rollback=2 outcome=rolled_back\n"
            "2026-08-07T11:00:00 workflow=AGENT/REPAIR event=repair_attempt "
            "attempt=1 plan=22600003 build=1 rollback=0 outcome=committed\n"
            "2026-08-07T12:00:00 workflow=AGENT/REPAIR event=repair_attempt "
            "attempt=1 plan=22600004 build=2 rollback=2 outcome=rolled_back\n"
            "2026-08-07T13:00:00 workflow=AGENT/REPAIR event=repair_attempt "
            "attempt=1 plan=22600005 build=1 rollback=0 outcome=committed\n"
            "2026-08-07T14:00:00 workflow=AGENT/REPAIR event=repair_attempt "
            "attempt=1 plan=22600006 build=2 rollback=2 outcome=rolled_back\n"
            "2026-08-07T14:01:00 workflow=AGENT/REPAIR event=repair_attempt "
            "attempt=2 plan=22600007 build=1 rollback=0 outcome=committed\n",
            file) == EOF) {
        (void)fclose(file);
        return 0;
    }

    return fclose(file) == 0;
}

int main(void)
{
    char output[32768];

    cleanup();
    openai_test_set_log_path(TEST_LOG);
    openai_test_set_history_limit(5U);

    if (!seed_history()) {
        (void)puts("M226 failed: unable to seed history.");
        cleanup();
        return EXIT_FAILURE;
    }

    if (!openai_first_text(
            "2",
            output,
            sizeof(output)) ||
        strstr(output, "FIRST 2 (oldest-first)") == NULL ||
        strstr(output, "22600002") == NULL ||
        strstr(output, "22600003") == NULL ||
        strstr(output, "22600001") != NULL ||
        strstr(output, "22600006") != NULL) {
        (void)puts("M226 failed: FIRST selection is incorrect.");
        cleanup();
        return EXIT_FAILURE;
    }

    if (!openai_last_text(
            "2",
            output,
            sizeof(output)) ||
        strstr(output, "LAST 2 (newest-first)") == NULL ||
        strstr(output, "22600006") == NULL ||
        strstr(output, "22600005") == NULL ||
        strstr(output, "22600004") != NULL) {
        (void)puts("M226 failed: LAST selection is incorrect.");
        cleanup();
        return EXIT_FAILURE;
    }

    if (!openai_range_text(
            "2 2",
            output,
            sizeof(output)) ||
        strstr(output, "RANGE 2 2 (newest-first)") == NULL ||
        strstr(output, "Match 2") == NULL ||
        strstr(output, "22600005") == NULL ||
        strstr(output, "Match 3") == NULL ||
        strstr(output, "22600004") == NULL ||
        strstr(output, "22600006") != NULL ||
        strstr(output, "22600003") != NULL) {
        (void)puts("M226 failed: RANGE selection is incorrect.");
        cleanup();
        return EXIT_FAILURE;
    }

    if (!openai_index_text(
            "3",
            output,
            sizeof(output)) ||
        strstr(output, "INDEX 3 (newest-first)") == NULL ||
        strstr(output, "Match 3") == NULL ||
        strstr(output, "22600004") == NULL ||
        strstr(output, "22600005") != NULL) {
        (void)puts("M226 failed: INDEX selection is incorrect.");
        cleanup();
        return EXIT_FAILURE;
    }

    if (openai_first_text(
            "0",
            output,
            sizeof(output)) ||
        openai_last_text(
            "6",
            output,
            sizeof(output)) ||
        openai_range_text(
            "5 2",
            output,
            sizeof(output)) ||
        openai_range_text(
            "2",
            output,
            sizeof(output)) ||
        openai_index_text(
            "6",
            output,
            sizeof(output)) ||
        openai_index_text(
            "abc",
            output,
            sizeof(output))) {
        (void)puts("M226 failed: invalid selection was accepted.");
        cleanup();
        return EXIT_FAILURE;
    }

    cleanup();
    (void)puts("Repair history selection bundle test passed.");
    return EXIT_SUCCESS;
}
