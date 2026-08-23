#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "LLM_JSON_PARSE.H"
#include "LLM_SSE.H"

#define M273_SSE_FIXTURE "[.BUILD]M273_SSE_FIXTURE.TXT"
#define M273_SSE_RESPONSE "[.BUILD]M273_SSE_RESPONSE.TXT"

static char streamed_text[256];

int llm_path_is_sensitive(const char *path)
{
    (void)path;
    return 0;
}

static void remove_all_versions(const char *path)
{
    if (path == NULL) {
        return;
    }

    while (remove(path) == 0) {
        /* Remove OpenVMS file versions newest first. */
    }
}

static void capture_text(const char *text)
{
    size_t used;
    size_t available;
    size_t length;

    if (text == NULL) {
        return;
    }

    used = strlen(streamed_text);
    if (used >= sizeof(streamed_text) - 1U) {
        return;
    }

    available = sizeof(streamed_text) - used - 1U;
    length = strlen(text);
    if (length > available) {
        length = available;
    }

    if (length != 0U) {
        (void)memcpy(streamed_text + used, text, length);
        streamed_text[used + length] = '\0';
    }
}

static int write_fixture(void)
{
    FILE *file;
    int success;

    remove_all_versions(M273_SSE_FIXTURE);
    file = fopen(M273_SSE_FIXTURE, "w");
    if (file == NULL) {
        return 0;
    }

    success =
        fputs("event: response.output_text.delta\n", file) != EOF &&
        fputs("data: {\"type\":\"response.output_text.delta\","
              "\"delta\":\"Hello \"}\n\n", file) != EOF &&
        fputs("event: response.output_text.delta\n", file) != EOF &&
        fputs("data: {\"type\":\"response.output_text.delta\","
              "\"delta\":\"world\"}\n\n", file) != EOF &&
        fputs("event: response.completed\n", file) != EOF &&
        fputs("data: {\"type\":\"response.completed\","
              "\"response\":{\"id\":\"resp_m273\","
              "\"object\":\"response\",\"status\":\"completed\","
              "\"output\":[{\"type\":\"message\","
              "\"content\":[{\"type\":\"output_text\","
              "\"text\":\"Hello world\"}]}],"
              "\"usage\":{\"input_tokens\":1,"
              "\"output_tokens\":2,\"total_tokens\":3}}}\n\n",
              file) != EOF;

    if (fclose(file) != 0) {
        success = 0;
    }

    return success;
}

static int test_parser(void)
{
    FILE *input;
    char *json;
    char *id;
    char *text;
    int emitted;
    int result;

    streamed_text[0] = '\0';
    remove_all_versions(M273_SSE_RESPONSE);

    if (!write_fixture()) {
        return 0;
    }

    input = fopen(M273_SSE_FIXTURE, "r");
    if (input == NULL) {
        return 0;
    }

    emitted = 0;
    result = llm_sse_parse(input,
                           capture_text,
                           M273_SSE_RESPONSE,
                           &emitted);

    if (fclose(input) != 0) {
        result = 0;
    }

    if (!result || !emitted ||
        strcmp(streamed_text, "Hello world") != 0) {
        return 0;
    }

    json = read_entire_file(M273_SSE_RESPONSE, NULL);
    if (json == NULL) {
        return 0;
    }

    id = extract_top_level_id(json);
    text = extract_output_text_from_json(json);

    result =
        id != NULL &&
        text != NULL &&
        strcmp(id, "resp_m273") == 0 &&
        strcmp(text, "Hello world") == 0;

    free(id);
    free(text);
    free(json);
    return result;
}

static int test_popen(void)
{
    FILE *pipe_stream;
    char line[256];
    int close_status;
    int found;

    (void)fflush(NULL);

    pipe_stream = popen(
        "WRITE SYS$OUTPUT \"M273_PIPE_OK\"",
        "r"
    );
    if (pipe_stream == NULL) {
        return 0;
    }

    (void)setvbuf(pipe_stream, NULL, _IONBF, 0);
    found = 0;

    while (fgets(line, sizeof(line), pipe_stream) != NULL) {
        if (strstr(line, "M273_PIPE_OK") != NULL) {
            found = 1;
        }
    }

    close_status = pclose(pipe_stream);
    return found && close_status != -1;
}

int main(void)
{
    int result;

    result = test_parser() && test_popen();

    remove_all_versions(M273_SSE_FIXTURE);
    remove_all_versions(M273_SSE_RESPONSE);

    if (!result) {
        (void)fprintf(
            stderr,
            "M273 streaming regression failed.\n"
        );
        return EXIT_FAILURE;
    }

    (void)puts("M273 SSE parser and popen regressions passed.");
    return EXIT_SUCCESS;
}
