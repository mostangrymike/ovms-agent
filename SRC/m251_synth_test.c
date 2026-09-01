#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "llm_internal.h"
#include "LLM_REQUEST_AGENT.H"
#include "LLM_RESPONSE.H"
#include "SETTINGS.H"

#define M279_LARGE_EDIT_TEST "M279_LARGE_EDIT.TMP"
#define M290_SETTINGS_TEST "[.BUILD]M251_SYNTH_SETTINGS.DAT"

int llm_m279_edit_read_ok(const char *path,
                          unsigned long minimum);

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

    file = fopen(LLM_REQUEST_FILE, "rb");

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

    file = fopen(LLM_RESPONSE_FILE, "w");
    if (file == NULL) {
        return 0;
    }

    if (fputs(
            "{\"output\":["
            "{\"type\":\"reasoning\",\"id\":\"rs_m279\"},"
            "{\"type\":\"function_call\","
            "\"call_id\":\"call_m251_last\","
            "\"name\":\"search_file\","
            "\"arguments\":\"{}\"},"
            "{\"type\":\"function_call\","
            "\"call_id\":\"call_m279_orphan\","
            "\"name\":\"read_file\","
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

static int test_empty_completed(void)
{
    static const char empty_response[] =
        "{\"status\":\"completed\",\"output\":[],"
        "\"usage\":{\"output_tokens\":7,\"total_tokens\":26}}";
    static const char nonempty_response[] =
        "{\"status\":\"completed\",\"output\":[{"
        "\"type\":\"message\"}],\"usage\":{\"output_tokens\":7}}";
    long tokens;

    tokens = 0L;
    if (!llm_response_empty_completed(empty_response, &tokens) ||
        tokens != 7L) {
        return 0;
    }

    tokens = 0L;
    return !llm_response_empty_completed(nonempty_response, &tokens);
}

static int test_tool_like_text(void)
{
    static const char read_text[] =
        "[[{\"name\":\"read_file_range\",\"parameters\":{"
        "\"path\":\"SRC/X.C\",\"start_line\":1,\"end_line\":2}}]]";
    static const char write_text[] =
        "{\"name\":\"replace_text\",\"arguments\":{"
        "\"path\":\"SRC/X.C\",\"old_text\":\"a\","
        "\"new_text\":\"b\"}}";
    char name[64];

    if (!llm_text_tool_like(read_text, name, sizeof(name)) ||
        strcmp(name, "read_file_range") != 0) {
        return 0;
    }

    if (!llm_text_tool_like(write_text, name, sizeof(name)) ||
        strcmp(name, "replace_text") != 0) {
        return 0;
    }

    if (llm_text_tool_like(
            "I will use read_file_range next.",
            name,
            sizeof(name))) {
        return 0;
    }

    return 1;
}

static int test_tool_model_compat(void)
{
    static const char tool_request[] =
        "{\"model\":\"test\",\"tools\":[]}";
    static const char plain_request[] =
        "{\"model\":\"test\",\"input\":\"hello\"}";

    if (!llm_m279_tool_model_ok(
            "https://api.groq.com/openai/v1",
            "openai/gpt-oss-120b")) {
        return 0;
    }

    if (!llm_m279_tool_model_ok(
            "https://api.groq.com/openai/v1",
            "openai/gpt-oss-20b")) {
        return 0;
    }

    if (!llm_m279_tool_model_ok(
            "https://api.groq.com/openai/v1",
            "qwen/qwen3-32b")) {
        return 0;
    }

    if (!llm_m279_tool_model_ok(
            "https://openrouter.ai/api/v1",
            "openai/gpt-oss-120b")) {
        return 0;
    }

    if (llm_m294_stream_ok(
            "https://api.groq.com/openai/v1",
            "openai/gpt-oss-120b",
            tool_request)) {
        return 0;
    }

    if (llm_m294_stream_ok(
            "https://api.groq.com/openai/v1",
            "openai/gpt-oss-20b",
            tool_request)) {
        return 0;
    }

    if (llm_m294_stream_ok(
            "https://api.groq.com/openai/v1",
            "qwen/qwen3.6-27b",
            tool_request)) {
        return 0;
    }

    if (!llm_m294_stream_ok(
            "https://api.groq.com/openai/v1",
            "openai/gpt-oss-20b",
            plain_request)) {
        return 0;
    }

    if (!llm_m294_stream_ok(
            "https://api.groq.com/openai/v1",
            "qwen/qwen3.6-27b",
            plain_request)) {
        return 0;
    }

    if (!llm_m294_stream_ok(
            "https://api.groq.com/openai/v1",
            "qwen/qwen3-32b",
            tool_request)) {
        return 0;
    }

    if (!llm_m294_stream_ok(
            "https://openrouter.ai/api/v1",
            "qwen/qwen3.6-27b",
            tool_request)) {
        return 0;
    }

    if (llm_m294_stream_ok(
            "https://openrouter.ai/api/v1",
            "openrouter/free",
            tool_request)) {
        return 0;
    }

    if (!llm_m294_stream_ok(
            "https://openrouter.ai/api/v1",
            "openrouter/free",
            plain_request)) {
        return 0;
    }

    if (!llm_m294_stream_ok(
            "https://openrouter.ai/api/v1",
            "openai/gpt-oss-120b",
            tool_request)) {
        return 0;
    }

    return 1;
}

static int request_has_default_limit(void)
{
    char *request;
    int ok;

    request = read_request();
    if (request == NULL) {
        return 0;
    }

    ok = strstr(request, "\"max_output_tokens\":2048") != NULL;
    free(request);
    return ok;
}

static int test_large_edit_reader(void)
{
    FILE *file;
    unsigned long index;
    int ok;

    remove_all(M279_LARGE_EDIT_TEST);
    file = fopen(M279_LARGE_EDIT_TEST, "wb");
    if (file == NULL) {
        return 0;
    }

    for (index = 0UL; index < 35000UL; ++index) {
        if (fputs("A\n", file) == EOF) {
            (void)fclose(file);
            remove_all(M279_LARGE_EDIT_TEST);
            return 0;
        }
    }

    if (fclose(file) != 0) {
        remove_all(M279_LARGE_EDIT_TEST);
        return 0;
    }

    ok = llm_m279_edit_read_ok(M279_LARGE_EDIT_TEST, 70000UL);
    remove_all(M279_LARGE_EDIT_TEST);
    return ok;
}

int main(void)
{
    char *request;
    int ok;

    remove_all(M290_SETTINGS_TEST);
    settings_test_path(M290_SETTINGS_TEST);
    if (!settings_reload()) {
        (void)puts("M290 failed: unable to isolate M251 settings.");
        return EXIT_FAILURE;
    }

    if (!test_nonempty_output()) {
        (void)puts(
            "M251 failed: empty output_text masked later synthesis."
        );
        return EXIT_FAILURE;
    }

    if (!test_empty_completed()) {
        (void)puts(
            "M279 failed: completed empty response classification was invalid."
        );
        return EXIT_FAILURE;
    }

    if (!test_tool_like_text()) {
        (void)puts(
            "M279 failed: tool-like assistant text classifier was invalid."
        );
        return EXIT_FAILURE;
    }

    if (!test_tool_model_compat()) {
        (void)puts(
            "M296 failed: provider tool/stream compatibility matrix invalid."
        );
        return EXIT_FAILURE;
    }

    if (!test_large_edit_reader()) {
        (void)puts(
            "M279 failed: guarded edit reader retained REVIEW size ceiling."
        );
        return EXIT_FAILURE;
    }

    remove_all(LLM_REQUEST_FILE);
    remove_all(LLM_RESPONSE_FILE);

    if (!write_agent_request_mode(
            "m251-test-model",
            "M251 test instructions",
            "M251 synthesis goal",
            NULL,
            NULL,
            NULL,
            0)) {
        (void)puts("M251 failed: initial local-context request failed.");
        remove_all(LLM_REQUEST_FILE);
        return EXIT_FAILURE;
    }

    if (!request_has_default_limit()) {
        (void)puts(
            "M279 failed: agent request omitted default output-token cap."
        );
        remove_all(LLM_REQUEST_FILE);
        return EXIT_FAILURE;
    }

    if (!write_response_fixture()) {
        (void)puts("M251 failed: unable to write response fixture.");
        remove_all(LLM_REQUEST_FILE);
        remove_all(LLM_RESPONSE_FILE);
        return EXIT_FAILURE;
    }

    if (!write_agent_final_request(
            "m251-test-model",
            "resp_m251_prev",
            "call_m251_last",
            "last tool result")) {
        (void)puts("M251 failed: final request writer returned failure.");
        remove_all(LLM_REQUEST_FILE);
        remove_all(LLM_RESPONSE_FILE);
        return EXIT_FAILURE;
    }

    request = read_request();

    if (request == NULL) {
        (void)puts("M251 failed: unable to read final request.");
        remove_all(LLM_REQUEST_FILE);
        remove_all(LLM_RESPONSE_FILE);
        return EXIT_FAILURE;
    }

    ok =
        strstr(request, "\"model\":\"m251-test-model\"") != NULL &&
        strstr(request, "\"previous_response_id\"") == NULL &&
        strstr(request, "M251 synthesis goal") != NULL &&
        strstr(request, "\"type\":\"reasoning\"") != NULL &&
        strstr(request,
            "\"type\":\"function_call\"") != NULL &&
        strstr(request,
            "\"type\":\"function_call_output\"") != NULL &&
        strstr(request, "\"call_id\":\"call_m251_last\"") != NULL &&
        strstr(request, "call_m279_orphan") == NULL &&
        strstr(request, "\"output\":\"last tool result\"") != NULL &&
        strstr(request, "Produce the final answer now") != NULL &&
        strstr(request, "\"max_output_tokens\":2048") != NULL &&
        strstr(request, "\"parallel_tool_calls\":false") != NULL &&
        strstr(request, "\"tools\"") == NULL &&
        strstr(request, "\"tool_choice\"") == NULL;

    free(request);
    remove_all(LLM_REQUEST_FILE);
    remove_all(LLM_RESPONSE_FILE);
    remove_all(M290_SETTINGS_TEST);
    settings_test_path(NULL);

    if (!ok) {
        (void)puts(
            "M251 failed: provider-neutral final synthesis request was invalid."
        );
        return EXIT_FAILURE;
    }

    (void)puts("M251 final synthesis request test passed.");
    (void)puts("M279 sibling tool-call filtering test passed.");
    (void)puts("M279 tool-like assistant text test passed.");
    (void)puts("M294 GPT-OSS Responses eligibility test passed.");
    (void)puts("M296 provider tool stream selection test passed.");
    (void)puts("M279 large guarded-edit reader test passed.");
    return EXIT_SUCCESS;
}
