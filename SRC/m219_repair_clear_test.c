#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "openai_internal.h"

#define TEST_LOG "M219_REPAIR_CLEAR.LOG"

/*
 * Link-only stubs required by OPENAI_PLAN.OBJ.
 * This deterministic clear-history test never uses interactive input.
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

static int append_text(const char *text)
{
    FILE *file;

    file = fopen(TEST_LOG, "a");
    if (file == NULL) {
        return 0;
    }

    if (fputs(text, file) == EOF) {
        (void)fclose(file);
        return 0;
    }

    return fclose(file) == 0;
}

static int read_text(char *output,
                     size_t output_size)
{
    FILE *file;
    size_t count;

    if (output == NULL || output_size == 0U) {
        return 0;
    }

    file = fopen(TEST_LOG, "r");
    if (file == NULL) {
        return 0;
    }

    count = fread(output, 1U, output_size - 1U, file);
    output[count] = '\0';

    if (ferror(file)) {
        (void)fclose(file);
        return 0;
    }

    return fclose(file) == 0;
}

int main(void)
{
    char text[4096];
    char summary[4096];

    cleanup();
    openai_test_set_log_path(TEST_LOG);

    if (!append_text(
            "2026-08-07T12:00:00 workflow=AGENT event=start status=1\n")) {
        (void)puts("M219 failed: unable to seed unrelated activity.");
        cleanup();
        return EXIT_FAILURE;
    }

    openai_log_repair_attempt(
        1U,
        0x21900001UL,
        2,
        2,
        "rolled_back"
    );
    openai_log_repair_attempt(
        2U,
        0x21900002UL,
        1,
        0,
        "committed"
    );

    if (openai_clear_repair_history(0)) {
        (void)puts("M219 failed: unapproved clear reported success.");
        cleanup();
        return EXIT_FAILURE;
    }

    if (!read_text(text, sizeof(text)) ||
        strstr(text, "21900001") == NULL ||
        strstr(text, "21900002") == NULL) {
        (void)puts("M219 failed: cancelled clear changed repair history.");
        cleanup();
        return EXIT_FAILURE;
    }

    if (!openai_clear_repair_history(1)) {
        (void)puts("M219 failed: approved clear failed.");
        cleanup();
        return EXIT_FAILURE;
    }

    if (!read_text(text, sizeof(text)) ||
        strstr(text, "workflow=AGENT event=start status=1") == NULL ||
        strstr(text, "workflow=AGENT/REPAIR") != NULL ||
        strstr(text, "21900001") != NULL ||
        strstr(text, "21900002") != NULL) {
        (void)puts("M219 failed: clear did not preserve unrelated activity.");
        cleanup();
        return EXIT_FAILURE;
    }

    if (openai_repair_status_text(summary, sizeof(summary)) ||
        openai_repair_history_text(summary, sizeof(summary)) ||
        openai_repair_stats_text(summary, sizeof(summary))) {
        (void)puts("M219 failed: repair summaries survived approved clear.");
        cleanup();
        return EXIT_FAILURE;
    }

    cleanup();
    (void)puts("Repair history clear test passed.");
    return EXIT_SUCCESS;
}
