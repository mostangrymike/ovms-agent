#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "agent.h"
#include "command_input_m149.inc"

#define TEST_LONG "M251_INPUT_LONG.TMP"
#define TEST_OVER "M251_INPUT_OVER.TMP"

static void remove_all(const char *path)
{
    while (remove(path) == 0) {
    }
}

static int test_extended_input(void)
{
    FILE *file;
    char input[OVMS_AGENT_INPUT_SIZE];
    size_t index;
    int status;
    int ok;

    remove_all(TEST_LONG);

    file = fopen(TEST_LONG, "w");
    if (file == NULL) {
        return 0;
    }

    if (fputs("AGENT ", file) == EOF) {
        (void)fclose(file);
        remove_all(TEST_LONG);
        return 0;
    }

    for (index = 0U; index < 2048U; ++index) {
        if (fputc('X', file) == EOF) {
            (void)fclose(file);
            remove_all(TEST_LONG);
            return 0;
        }
    }

    if (fputc('\n', file) == EOF || fclose(file) != 0) {
        remove_all(TEST_LONG);
        return 0;
    }

    file = fopen(TEST_LONG, "r");
    if (file == NULL) {
        remove_all(TEST_LONG);
        return 0;
    }

    status = command_read_stream(file, input, sizeof(input));
    (void)fclose(file);

    ok =
        status > 0 &&
        strlen(input) == 2055U &&
        strncmp(input, "AGENT ", 6U) == 0 &&
        input[2054] == '\n';

    remove_all(TEST_LONG);
    return ok;
}

static int test_capacity_recovery(void)
{
    FILE *file;
    char input[OVMS_AGENT_INPUT_SIZE];
    size_t index;
    int first_status;
    int second_status;
    int ok;

    remove_all(TEST_OVER);

    file = fopen(TEST_OVER, "w");
    if (file == NULL) {
        return 0;
    }

    if (fputs("AGENT ", file) == EOF) {
        (void)fclose(file);
        remove_all(TEST_OVER);
        return 0;
    }

    for (index = 0U;
         index < (size_t)OVMS_AGENT_INPUT_SIZE + 32U;
         ++index) {
        if (fputc('Y', file) == EOF) {
            (void)fclose(file);
            remove_all(TEST_OVER);
            return 0;
        }
    }

    if (fputs("\nSTATUS\n", file) == EOF || fclose(file) != 0) {
        remove_all(TEST_OVER);
        return 0;
    }

    file = fopen(TEST_OVER, "r");
    if (file == NULL) {
        remove_all(TEST_OVER);
        return 0;
    }

    first_status = command_read_stream(file, input, sizeof(input));
    second_status = command_read_stream(file, input, sizeof(input));
    (void)fclose(file);

    ok =
        first_status < 0 &&
        second_status > 0 &&
        strcmp(input, "STATUS\n") == 0;

    remove_all(TEST_OVER);
    return ok;
}

int main(void)
{
    if (OVMS_AGENT_INPUT_SIZE != 8192) {
        (void)puts("M251.5a failed: input size is not 8192.");
        return EXIT_FAILURE;
    }

    if (!test_extended_input()) {
        (void)puts("M251.5a failed: extended input was rejected or changed.");
        return EXIT_FAILURE;
    }

    if (!test_capacity_recovery()) {
        (void)puts("M251.5a failed: overflow recovery failed.");
        return EXIT_FAILURE;
    }

    (void)puts("M251.5a extended input regression passed.");
    return EXIT_SUCCESS;
}
