#include <stdio.h>
#include <stdlib.h>

#include "llm_internal.h"

#define TEST_EMPTY "M251_EMPTY.TMP"
#define TEST_FENCE "M251_FENCE.TMP"

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

static int file_exists(const char *path)
{
    FILE *file;

    file = fopen(path, "r");

    if (file == NULL) {
        return 0;
    }

    (void)fclose(file);
    return 1;
}

static int test_empty(void)
{
    static const char args[] =
        "{\"path\":\"M251_EMPTY.TMP\",\"content\":\"\"}";
    char *display_path;
    llm_create_result result;
    int ok;

    remove_all(TEST_EMPTY);
    display_path = NULL;

    result = execute_create_file_tool(args, &display_path);

    ok =
        result == LLM_CREATE_ERROR &&
        display_path != NULL &&
        !file_exists(TEST_EMPTY);

    free(display_path);
    remove_all(TEST_EMPTY);
    return ok;
}

static int test_fence(void)
{
    static const char args[] =
        "{\"path\":\"M251_FENCE.TMP\",\"content\":\"```\"}";
    char *display_path;
    llm_create_result result;
    int ok;

    remove_all(TEST_FENCE);
    display_path = NULL;

    result = execute_create_file_tool(args, &display_path);

    ok =
        result == LLM_CREATE_ERROR &&
        display_path != NULL &&
        !file_exists(TEST_FENCE);

    free(display_path);
    remove_all(TEST_FENCE);
    return ok;
}

int main(void)
{
    if (!test_empty()) {
        (void)puts("M251 failed: empty create content was not rejected.");
        return EXIT_FAILURE;
    }

    if (!test_fence()) {
        (void)puts("M251 failed: Markdown fence content was not rejected.");
        return EXIT_FAILURE;
    }

    (void)puts("M251 create-content validation test passed.");
    return EXIT_SUCCESS;
}
