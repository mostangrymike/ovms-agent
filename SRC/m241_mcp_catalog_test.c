#include "openai_internal.h"

/* Regression-test stubs for command-layer helpers pulled in indirectly
 * through the modular OpenAI/command link set.  M239 established this
 * pattern for standalone milestone test images. */
int command_line_complete(const char *input, size_t length, int eof)
{
    (void)input;
    (void)length;
    (void)eof;
    return 0;
}

int command_read_stream(FILE *stream, char *buffer, size_t buffer_size)
{
    (void)stream;
    (void)buffer;
    (void)buffer_size;
    return 0;
}

static int require_true(int condition, const char *message)
{
    if (!condition) {
        (void)fprintf(stderr, "M241 MCP catalog regression failed: %s\n", message);
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
        "bad entry";

    if (!require_true(
            openai_mcp_catalog_text(config, output, sizeof(output)),
            "catalog text generation")) {
        return 1;
    }
    if (!require_true(
            strstr(output, "Configured servers: 2") != NULL,
            "configured server count")) {
        return 1;
    }
    if (!require_true(
            strstr(output, "Ignored entries:    1") != NULL,
            "invalid entry count")) {
        return 1;
    }
    if (!require_true(
            strstr(output, "docs") != NULL &&
            strstr(output, "transport=stdio") != NULL &&
            strstr(output, "@MCP_DOCS") != NULL,
            "stdio server inventory")) {
        return 1;
    }
    if (!require_true(
            strstr(output, "issues") != NULL &&
            strstr(output, "transport=http") != NULL,
            "http server inventory")) {
        return 1;
    }

    if (!require_true(
            openai_mcp_info_text(
                config, "DOCS", output, sizeof(output)),
            "case-insensitive server lookup")) {
        return 1;
    }
    if (!require_true(
            strstr(output, "Name:      docs") != NULL &&
            strstr(output, "State:     configured") != NULL,
            "server information output")) {
        return 1;
    }

    if (!require_true(
            !openai_mcp_info_text(
                config, "missing", output, sizeof(output)),
            "unknown server rejection")) {
        return 1;
    }

    if (!require_true(
            openai_mcp_catalog_text(NULL, output, sizeof(output)) &&
            strstr(output, "Configured servers: 0") != NULL &&
            strstr(output, "(none configured)") != NULL,
            "empty configuration")) {
        return 1;
    }

    (void)puts("M241 MCP catalog regression passed.");
    return 0;
}
