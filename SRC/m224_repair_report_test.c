#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "openai_internal.h"

#define TEST_LOG "M224_REPAIR_REPORT.LOG"
#define TEST_CSV "M224_REPAIR_REPORT.CSV"
#define TEST_KV  "M224_REPAIR_REPORT.KV"

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
    remove_all(TEST_CSV);
    remove_all(TEST_KV);
}

static int seed_history(void)
{
    FILE *file;

    file = fopen(TEST_LOG, "w");
    if (file == NULL) {
        return 0;
    }

    if (fputs(
            "2026-08-07T10:00:00 workflow=AGENT/REPAIR event=repair_attempt "
            "attempt=1 plan=22400001 build=1 rollback=0 outcome=committed\n"
            "2026-08-07T11:00:00 workflow=AGENT/REPAIR event=repair_attempt "
            "attempt=1 plan=22400002 build=2 rollback=2 outcome=rolled_back\n"
            "2026-08-07T11:01:00 workflow=AGENT/REPAIR event=repair_attempt "
            "attempt=2 plan=22400003 build=1 rollback=0 outcome=committed\n"
            "2026-08-07T12:00:00 workflow=AGENT/REPAIR event=repair_attempt "
            "attempt=1 plan=22400004 build=2 rollback=2 outcome=rolled_back\n"
            "2026-08-07T12:01:00 workflow=AGENT/REPAIR event=repair_attempt "
            "attempt=2 plan=22400005 build=2 rollback=2 outcome=rolled_back\n"
            "2026-08-07T13:00:00 workflow=AGENT/REPAIR event=repair_attempt "
            "attempt=1 plan=22400006 build=2 rollback=3 outcome=unsafe\n",
            file) == EOF) {
        (void)fclose(file);
        return 0;
    }

    return fclose(file) == 0;
}

static int read_file(const char *path,
                     char *output,
                     size_t output_size)
{
    FILE *file;
    size_t count;

    file = fopen(path, "r");
    if (file == NULL) {
        return 0;
    }

    count = fread(
        output,
        1U,
        output_size - 1U,
        file
    );
    output[count] = '\0';

    if (ferror(file)) {
        (void)fclose(file);
        return 0;
    }

    return fclose(file) == 0;
}

static int seed_protected(const char *path)
{
    FILE *file;

    file = fopen(path, "w");
    if (file == NULL) {
        return 0;
    }

    if (fputs("protected\n", file) == EOF) {
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
    openai_test_set_history_limit(4U);

    if (!seed_history()) {
        (void)puts("M224 failed: unable to seed history.");
        cleanup();
        return EXIT_FAILURE;
    }

    if (!openai_report_text(
            output,
            sizeof(output)) ||
        strstr(output, "History window: 4") == NULL ||
        strstr(output, "Runs available: 4") == NULL ||
        strstr(output, "22400006") == NULL ||
        strstr(output, "22400001") == NULL) {
        (void)puts("M224 failed: report output is incorrect.");
        cleanup();
        return EXIT_FAILURE;
    }

    if (!openai_summary_text(
            output,
            sizeof(output)) ||
        strstr(output, "Runs analyzed:    4") == NULL ||
        strstr(output, "Committed:        2") == NULL ||
        strstr(output, "Rolled back:      1") == NULL ||
        strstr(output, "Unsafe:           1") == NULL ||
        strstr(output, "One attempt:      2") == NULL ||
        strstr(output, "Two attempts:     2") == NULL ||
        strstr(output, "Success rate:     50%") == NULL) {
        (void)puts("M224 failed: summary output is incorrect.");
        cleanup();
        return EXIT_FAILURE;
    }

    if (!openai_csv_file(TEST_CSV, 0) ||
        !read_file(TEST_CSV, output, sizeof(output)) ||
        strstr(output, "run,started,attempt,plan") == NULL ||
        strstr(output, "1,2026-08-07T13:00:00,1,22400006") == NULL ||
        strstr(output, "4,2026-08-07T10:00:00,1,22400001") == NULL) {
        (void)puts("M224 failed: CSV export is incorrect.");
        cleanup();
        return EXIT_FAILURE;
    }

    if (!openai_kv_file(TEST_KV, 0) ||
        !read_file(TEST_KV, output, sizeof(output)) ||
        strstr(output, "history.window=4") == NULL ||
        strstr(output, "history.runs=4") == NULL ||
        strstr(output, "run.1.started=2026-08-07T13:00:00") == NULL ||
        strstr(output, "run.4.attempt.1.plan=22400001") == NULL) {
        (void)puts("M224 failed: KV export is incorrect.");
        cleanup();
        return EXIT_FAILURE;
    }

    if (!seed_protected(TEST_CSV) ||
        openai_csv_file(TEST_CSV, 0) ||
        !read_file(TEST_CSV, output, sizeof(output)) ||
        strcmp(output, "protected\n") != 0) {
        (void)puts("M224 failed: CSV overwrite protection failed.");
        cleanup();
        return EXIT_FAILURE;
    }

    if (!openai_csv_file(TEST_CSV, 1)) {
        (void)puts("M224 failed: approved CSV overwrite failed.");
        cleanup();
        return EXIT_FAILURE;
    }

    if (openai_csv_file("../M224_ESCAPE.CSV", 0) ||
        openai_kv_file("M224*.KV", 0) ||
        openai_kv_file("M224;1", 0)) {
        (void)puts("M224 failed: unsafe export filespec was accepted.");
        cleanup();
        return EXIT_FAILURE;
    }

    cleanup();
    (void)puts("Repair reporting export bundle test passed.");
    return EXIT_SUCCESS;
}
