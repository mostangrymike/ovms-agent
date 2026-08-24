#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "LLM_JSON_PARSE.H"
#include "LLM_SSE.H"
#include "LLM_USAGE.H"

#define M273_SSE_FIXTURE "[.BUILD]M273_SSE_FIXTURE.TXT"
#define M273_SSE_RESPONSE "[.BUILD]M273_SSE_RESPONSE.TXT"
#define M273_PIPE_CHILD "@[.BUILD]M273_STREAM_CHILD.COM"
#define M273_PIPE_SENTINEL "[.BUILD]M273_STREAM_CHILD.DONE"

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

static int file_exists(const char *path)
{
    FILE *file;
    int result;

    if (path == NULL) {
        return 0;
    }

    file = fopen(path, "r");
    if (file == NULL) {
        return 0;
    }

    result = fclose(file) == 0;
    return result;
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

static int write_fixture(int blank_separators)
{
    FILE *file;
    const char *separator;
    int success;

    remove_all_versions(M273_SSE_FIXTURE);
    file = fopen(M273_SSE_FIXTURE, "w");
    if (file == NULL) {
        return 0;
    }

    separator = blank_separators ? "\n\n" : "\n";

    success =
        fputs("event: response.output_text.delta\n", file) != EOF &&
        fputs("data: {\"type\":\"response.output_text.delta\","
              "\"delta\":\"Hello \"}", file) != EOF &&
        fputs(separator, file) != EOF &&
        fputs("event: response.output_text.delta\n", file) != EOF &&
        fputs("data: {\"type\":\"response.output_text.delta\","
              "\"delta\":\"world\"}", file) != EOF &&
        fputs(separator, file) != EOF &&
        fputs("event: response.completed\n", file) != EOF &&
        fputs("data: {\"type\":\"response.completed\","
              "\"response\":{\"id\":\"resp_m273\","
              "\"object\":\"response\",\"status\":\"completed\","
              "\"output\":[{\"type\":\"message\","
              "\"content\":[{\"type\":\"output_text\","
              "\"text\":\"Hello world\"}]}],"
              "\"usage\":{\"input_tokens\":1,"
              "\"output_tokens\":2,\"total_tokens\":3}}}",
              file) != EOF &&
        fputs(separator, file) != EOF;

    if (fclose(file) != 0) {
        success = 0;
    }

    return success;
}

static int write_fragmented_fixture(void)
{
    static const char wire[] =
        "event: response.output_text.delta"
        "data: {\"type\":\"response.output_text.delta\","
        "\"delta\":\"Hello \"}"
        "event: response.output_text.delta"
        "data: {\"type\":\"response.output_text.delta\","
        "\"delta\":\"world\"}"
        "event: response.completed"
        "data: {\"type\":\"response.completed\","
        "\"response\":{\"id\":\"resp_m273\","
        "\"object\":\"response\",\"status\":\"completed\","
        "\"output\":[{\"type\":\"message\","
        "\"content\":[{\"type\":\"output_text\","
        "\"text\":\"Hello world\"}]}],"
        "\"usage\":{\"input_tokens\":1,"
        "\"output_tokens\":2,\"total_tokens\":3}}}";
    FILE *file;
    size_t index;
    int success;

    remove_all_versions(M273_SSE_FIXTURE);
    file = fopen(M273_SSE_FIXTURE, "w");
    if (file == NULL) {
        return 0;
    }

    success = 1;
    for (index = 0U; wire[index] != '\0'; ++index) {
        if (fputc((unsigned char)wire[index], file) == EOF ||
            fputc('\n', file) == EOF) {
            success = 0;
            break;
        }
    }

    if (fclose(file) != 0) {
        success = 0;
    }

    return success;
}

static int verify_parser_result(void)
{
    FILE *input;
    char *json;
    char *id;
    char *text;
    int emitted;
    int result;

    streamed_text[0] = '\0';
    remove_all_versions(M273_SSE_RESPONSE);

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

static int test_parser_case(int blank_separators)
{
    if (!write_fixture(blank_separators)) {
        return 0;
    }

    return verify_parser_result();
}

static int test_fragmented_parser(void)
{
    if (!write_fragmented_fixture()) {
        return 0;
    }

    return verify_parser_result();
}

static int test_usage(void)
{
    static const char second_response[] =
        "{\"output\":[{\"type\":\"message\","
        "\"content\":[{\"type\":\"output_text\","
        "\"text\":\"fake \\\"usage\\\": key\"}]}],"
        "\"usage\":{\"input_tokens\":4,"
        "\"output_tokens\":6,\"total_tokens\":10}}";
    static const char no_usage[] =
        "{\"id\":\"resp_no_usage\",\"status\":\"completed\"}";
    llm_usage_stats stats;
    char status[160];

    if (!llm_usage_record_file(M273_SSE_RESPONSE)) {
        return 0;
    }

    llm_usage_get(&stats);
    if (!stats.last_known ||
        stats.requests != 1UL ||
        stats.last_input != 1UL ||
        stats.last_output != 2UL ||
        stats.last_total != 3UL ||
        stats.session_input != 1UL ||
        stats.session_output != 2UL ||
        stats.session_total != 3UL) {
        return 0;
    }

    if (!llm_usage_record_json(second_response)) {
        return 0;
    }

    llm_usage_get(&stats);
    if (!stats.last_known ||
        stats.requests != 2UL ||
        stats.last_input != 4UL ||
        stats.last_output != 6UL ||
        stats.last_total != 10UL ||
        stats.session_input != 5UL ||
        stats.session_output != 8UL ||
        stats.session_total != 13UL) {
        return 0;
    }

    if (!llm_usage_status(status, sizeof(status), 2U, 4U) ||
        strstr(status, "turn 2/4") == NULL ||
        strstr(status, "in=4") == NULL ||
        strstr(status, "out=6") == NULL ||
        strstr(status, "total=10") == NULL ||
        strstr(status, "session=13") == NULL) {
        return 0;
    }

    if (llm_usage_record_json(no_usage)) {
        return 0;
    }

    llm_usage_get(&stats);
    return !stats.last_known &&
           stats.requests == 2UL &&
           stats.session_input == 5UL &&
           stats.session_output == 8UL &&
           stats.session_total == 13UL;
}

static int test_popen(void)
{
    FILE *pipe_stream;
    char line[256];
    int close_status;
    int found_first;
    int found_second;
    int delivered_early;
    int sentinel_after;

    remove_all_versions(M273_PIPE_SENTINEL);
    (void)fflush(NULL);

    pipe_stream = popen(M273_PIPE_CHILD, "r");
    if (pipe_stream == NULL) {
        return 0;
    }

    (void)setvbuf(pipe_stream, NULL, _IONBF, 0);
    found_first = 0;

    while (!found_first &&
           fgets(line, sizeof(line), pipe_stream) != NULL) {
        if (strstr(line, "M273_PIPE_FIRST") != NULL) {
            found_first = 1;
        }
    }

    if (!found_first) {
        (void)pclose(pipe_stream);
        remove_all_versions(M273_PIPE_SENTINEL);
        return 0;
    }

    /*
     * The child creates the sentinel only after a deliberate WAIT.
     * If the first line is not readable until the child has already
     * advanced past that WAIT, this is eventual delivery, not stream
     * delivery, and the regression must fail.
     */
    delivered_early = !file_exists(M273_PIPE_SENTINEL);
    found_second = 0;

    while (fgets(line, sizeof(line), pipe_stream) != NULL) {
        if (strstr(line, "M273_PIPE_SECOND") != NULL) {
            found_second = 1;
        }
    }

    close_status = pclose(pipe_stream);
    sentinel_after = file_exists(M273_PIPE_SENTINEL);
    remove_all_versions(M273_PIPE_SENTINEL);

    return delivered_early &&
           found_second &&
           sentinel_after &&
           close_status != -1;
}

int main(void)
{
    int result;

    result =
        test_parser_case(1) &&
        test_parser_case(0) &&
        test_fragmented_parser() &&
        test_usage() &&
        test_popen();

    remove_all_versions(M273_SSE_FIXTURE);
    remove_all_versions(M273_SSE_RESPONSE);
    remove_all_versions(M273_PIPE_SENTINEL);

    if (!result) {
        (void)fprintf(
            stderr,
            "M273 streaming/usage regression failed.\n"
        );
        return EXIT_FAILURE;
    }

    (void)puts(
        "M273 SSE, usage, and incremental popen regressions passed."
    );
    return EXIT_SUCCESS;
}
