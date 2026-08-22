#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "llm_internal.h"

#define TEST_LOG "M215_REPAIR_EXPORT.LOG"
#define TEST_OUT "M215_REPAIR_EXPORT.TXT"

/*
 * Link-only stubs required by LLM_PLAN.OBJ.
 * This deterministic export test never uses interactive input.
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

    while (remove(TEST_OUT) == 0) {
    }
}

static int read_file(const char *path,
                     char *buffer,
                     size_t buffer_size)
{
    FILE *file;
    size_t count;

    if (buffer == NULL || buffer_size == 0U) {
        return 0;
    }

    file = fopen(path, "r");
    if (file == NULL) {
        return 0;
    }

    count = fread(buffer, 1U, buffer_size - 1U, file);
    buffer[count] = '\0';

    if (ferror(file)) {
        (void)fclose(file);
        return 0;
    }

    return fclose(file) == 0;
}

static int write_seed(const char *path,
                      const char *text)
{
    FILE *file;

    file = fopen(path, "w");
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
    char text[8192];

    cleanup();
    llm_test_set_log_path(TEST_LOG);

    llm_log_repair_attempt(
        1U,
        0x11112222UL,
        2,
        LLM_ROLLBACK_SUCCEEDED,
        "rolled_back"
    );
    llm_log_repair_attempt(
        2U,
        0x33334444UL,
        1,
        LLM_ROLLBACK_NONE,
        "committed"
    );

    if (!llm_repair_export_file(TEST_OUT, 0) ||
        !read_file(TEST_OUT, text, sizeof(text)) ||
        strstr(text, "OVMS Agent repair history") == NULL ||
        strstr(text, "11112222") == NULL ||
        strstr(text, "33334444") == NULL ||
        strstr(text, "Final outcome: committed") == NULL) {
        (void)puts("M215 failed: successful history export is incorrect.");
        cleanup();
        return EXIT_FAILURE;
    }

    if (llm_repair_export_file("../M215_ESCAPE.TXT", 0) ||
        llm_repair_export_file("M215*.TXT", 0) ||
        llm_repair_export_file("M215;1", 0)) {
        (void)puts("M215 failed: unsafe export filespec was accepted.");
        cleanup();
        return EXIT_FAILURE;
    }

    if (!write_seed(TEST_OUT, "protected\n")) {
        (void)puts("M215 failed: unable to seed protected export.");
        cleanup();
        return EXIT_FAILURE;
    }

    if (llm_repair_export_file(TEST_OUT, 0) ||
        !read_file(TEST_OUT, text, sizeof(text)) ||
        strcmp(text, "protected\n") != 0) {
        (void)puts("M215 failed: existing export was overwritten.");
        cleanup();
        return EXIT_FAILURE;
    }

    if (!llm_repair_export_file(TEST_OUT, 1) ||
        !read_file(TEST_OUT, text, sizeof(text)) ||
        strstr(text, "OVMS Agent repair history") == NULL) {
        (void)puts("M215 failed: approved overwrite did not succeed.");
        cleanup();
        return EXIT_FAILURE;
    }

    cleanup();
    (void)puts("Repair history export test passed.");
    return EXIT_SUCCESS;
}
