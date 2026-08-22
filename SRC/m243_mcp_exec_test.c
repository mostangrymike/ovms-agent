#include "llm_internal.h"

int command_line_complete(const char *input, size_t length, int eof)
{
    (void)input; (void)length; (void)eof; return 0;
}

int command_read_stream(FILE *stream, char *buffer, size_t buffer_size)
{
    (void)stream; (void)buffer; (void)buffer_size; return 0;
}

typedef struct exec_probe {
    int calls;
    int succeed;
    char target[128];
    char tool[64];
    char arguments[256];
} exec_probe;

static int fake_executor(const char *target,
                         const char *tool,
                         const char *arguments,
                         char *result,
                         size_t result_size,
                         void *context)
{
    exec_probe *probe = (exec_probe *)context;

    ++probe->calls;
    (void)snprintf(probe->target, sizeof(probe->target), "%s", target);
    (void)snprintf(probe->tool, sizeof(probe->tool), "%s", tool);
    (void)snprintf(probe->arguments, sizeof(probe->arguments), "%s", arguments);

    if (!probe->succeed) {
        return 0;
    }

    (void)snprintf(result, result_size,
                   "bridge-result tool=%s args=%s\n", tool, arguments);
    return 1;
}

static int require_true(int condition, const char *message)
{
    if (!condition) {
        (void)fprintf(stderr,
            "M243 MCP execution regression failed: %s\n", message);
        return 0;
    }
    return 1;
}

int main(void)
{
    char output[4096];
    exec_probe probe;
    const char *config;

    config =
        "docs|stdio|@MCP_DOCS;"
        "issues|http|https://tools.example.test/mcp;"
        "bad|stdio|@MCP_DOCS;DELETE";

    memset(&probe, 0, sizeof(probe));
    probe.succeed = 1;

    llm_test_reset_approval();
    if (!require_true(
            llm_mcp_execute_text(
                config, "docs search RMS", fake_executor, &probe,
                output, sizeof(output)) &&
            strstr(output, "FULL approval policy is required") != NULL &&
            probe.calls == 0,
            "read-only policy refusal")) {
        return 1;
    }

    if (!require_true(llm_set_approval("full"),
                      "enable full approval")) {
        return 1;
    }

    if (!require_true(
            llm_mcp_execute_text(
                config, "docs search {\\\"query\\\":\\\"RMS\\\"}",
                fake_executor, &probe, output, sizeof(output)) &&
            probe.calls == 1 &&
            strcmp(probe.target, "@MCP_DOCS") == 0 &&
            strcmp(probe.tool, "search") == 0 &&
            strstr(probe.arguments, "query") != NULL &&
            strstr(output, "Status:     success") != NULL &&
            strstr(output, "bridge-result") != NULL,
            "approved stdio execution")) {
        return 1;
    }

    if (!require_true(
            llm_mcp_execute_text(
                config, "issues get 42", fake_executor, &probe,
                output, sizeof(output)) &&
            strstr(output, "transport http is not enabled") != NULL &&
            probe.calls == 1,
            "http refusal")) {
        return 1;
    }

    if (!require_true(
            llm_mcp_execute_text(
                config, "bad search x", fake_executor, &probe,
                output, sizeof(output)) &&
            strstr(output, "unsafe stdio bridge target") != NULL &&
            probe.calls == 1,
            "unsafe target refusal")) {
        return 1;
    }

    probe.succeed = 0;
    if (!require_true(
            llm_mcp_execute_text(
                config, "docs search fail", fake_executor, &probe,
                output, sizeof(output)) &&
            strstr(output, "execution failed") != NULL &&
            probe.calls == 2,
            "executor failure reporting")) {
        return 1;
    }

    llm_test_reset_approval();
    (void)puts("M243 MCP execution regression passed.");
    return 0;
}
