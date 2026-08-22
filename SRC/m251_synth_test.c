#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "llm_internal.h"
#include "LLM_REQUEST_AGENT.H"

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

static char *read_request(void)
{
    FILE *file;
    long length;
    char *text;
    size_t read_count;

    file = fopen(OPENAI_REQUEST_FILE, "rb");

    if (file == NULL) {
        return NULL;
    }

    if (fseek(file, 0L, SEEK_END) != 0) {
        (void)fclose(file);
        return NULL;
    }

    length = ftell(file);

    if (length < 0L || fseek(file, 0L, SEEK_SET) != 0) {
        (void)fclose(file);
        return NULL;
    }

    text = (char *)malloc((size_t)length + 1U);

    if (text == NULL) {
        (void)fclose(file);
        return NULL;
    }

    read_count = fread(text, 1U, (size_t)length, file);
    (void)fclose(file);

    if (read_count != (size_t)length) {
        free(text);
        return NULL;
    }

    text[read_count] = '\0';
    return text;
}

static int write_response_fixture(void)
{
    FILE *file;

    file = fopen(OPENAI_RESPONSE_FILE, "w");
    if (file == NULL) {
        return 0;
    }

    if (fputs(
            "{\"output\":[{\"type\":\"function_call\","
            "\"call_id\":\"call_m251_last\","
            "\"name\":\"search_file\","
            "\"arguments\":\"{}\"}]}\n",
            file) == EOF) {
        (void)fclose(file);
        return 0;
    }

    return fclose(file) == 0;
}

static int test_nonempty_output(void)
{
    static const char response[] =
        "{\"output\":["
        "{\"type\":\"message\",\"content\":["
        "{\"type\":\"output_text\",\"text\":\"\"}]},"
        "{\"type\":\"message\",\"content\":["
        "{\"type\":\"output_text\",\"text\":\"final synthesis\"}]}"
        "]}";
    char *text;
    int ok;

    text = extract_output_text_from_json(response);

    ok = text != NULL && strcmp(text, "final synthesis") == 0;
    free(text);
    return ok;
}

int main(void)
{
    char *request;
    int ok;

    if (!test_nonempty_output()) {
        (void)puts(
            "M251 failed: empty output_text masked later synthesis."
        );
        return EXIT_FAILURE;
    }

    remove_all(OPENAI_REQUEST_FILE);
    remove_all(OPENAI_RESPONSE_FILE);

    if (!write_agent_request_mode(
            "m251-test-model",
            "M251 test instructions",
            "M251 synthesis goal",
            NULL,
            NULL,
            NULL,
            0)) {
        (void)puts("M251 failed: initial local-context request failed.");
        remove_all(OPENAI_REQUEST_FILE);
        return EXIT_FAILURE;
    }

    if (!write_response_fixture()) {
        (void)puts("M251 failed: unable to write response fixture.");
        remove_all(OPENAI_REQUEST_FILE);
        remove_all(OPENAI_RESPONSE_FILE);
        return EXIT_FAILURE;
    }

    if (!write_agent_final_request(
            "m251-test-model",
            "resp_m251_prev",
            "call_m251_last",
            "last tool result")) {
        (void)puts("M251 failed: final request writer returned failure.");
        remove_all(OPENAI_REQUEST_FILE);
        remove_all(OPENAI_RESPONSE_FILE);
        return EXIT_FAILURE;
    }

    request = read_request();

    if (request == NULL) {
        (void)puts("M251 failed: unable to read final request.");
        remove_all(OPENAI_REQUEST_FILE);
        remove_all(OPENAI_RESPONSE_FILE);
        return EXIT_FAILURE;
    }

    ok =
        strstr(request, "\"model\":\"m251-test-model\"") != NULL &&
        strstr(request, "\"previous_response_id\"") == NULL &&
        strstr(request, "M251 synthesis goal") != NULL &&
        strstr(request,
            "\"type\":\"function_call\"") != NULL &&
        strstr(request,
            "\"type\":\"function_call_output\"") != NULL &&
        strstr(request, "\"call_id\":\"call_m251_last\"") != NULL &&
        strstr(request, "\"output\":\"last tool result\"") != NULL &&
        strstr(request, "Produce the final answer now") != NULL &&
        strstr(request, "\"parallel_tool_calls\":false") != NULL &&
        strstr(request, "\"tools\"") == NULL &&
        strstr(request, "\"tool_choice\"") == NULL;

    free(request);
    remove_all(OPENAI_REQUEST_FILE);
    remove_all(OPENAI_RESPONSE_FILE);

    if (!ok) {
        (void)puts(
            "M251 failed: provider-neutral final synthesis request was invalid."
        );
        return EXIT_FAILURE;
    }

    (void)puts("M251 final synthesis request test passed.");
    return EXIT_SUCCESS;
}
