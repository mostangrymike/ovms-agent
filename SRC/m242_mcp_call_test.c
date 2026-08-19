#include "llm_internal.h"

int command_line_complete(const char *input, size_t length, int eof)
{
    (void)input; (void)length; (void)eof; return 0;
}

int command_read_stream(FILE *stream, char *buffer, size_t buffer_size)
{
    (void)stream; (void)buffer; (void)buffer_size; return 0;
}

static int require_true(int condition, const char *message)
{
    if (!condition) {
        (void)fprintf(stderr,
            "M242 MCP call regression failed: %s\n", message);
        return 0;
    }
    return 1;
}

int main(void)
{
    char output[4096];
    const char *config;

    config =
        "docs|stdio|@MCP_DOCS;"
        "issues|http|https://tools.example.test/mcp;"
        "events|sse|https://events.example.test/mcp";

    if (!require_true(
            openai_mcp_call_text(
                config,
                "docs search {\\\"query\\\":\\\"RMS\\\"}",
                output, sizeof(output)),
            "stdio call planning")) {
        return 1;
    }
    if (!require_true(
            strstr(output, "Server:     docs") != NULL &&
            strstr(output, "Tool:       search") != NULL &&
            strstr(output, "guarded subprocess transport") != NULL &&
            strstr(output, "Approval:   required") != NULL &&
            strstr(output, "Execution:  not performed") != NULL,
            "stdio policy output")) {
        return 1;
    }

    if (!require_true(
            openai_mcp_call_text(
                config, "ISSUES get_issue 42",
                output, sizeof(output)) &&
            strstr(output, "guarded HTTP transport") != NULL,
            "http call planning")) {
        return 1;
    }

    if (!require_true(
            openai_mcp_call_text(
                config, "events subscribe",
                output, sizeof(output)) &&
            strstr(output, "guarded SSE transport") != NULL &&
            strstr(output, "Arguments:  (none)") != NULL,
            "sse call planning")) {
        return 1;
    }

    if (!require_true(
            !openai_mcp_call_text(
                config, "missing search x", output, sizeof(output)),
            "unknown server rejection")) {
        return 1;
    }

    if (!require_true(
            !openai_mcp_call_text(
                config, "docs bad/tool x", output, sizeof(output)),
            "unsafe tool-name rejection")) {
        return 1;
    }

    (void)puts("M242 MCP call regression passed.");
    return 0;
}
