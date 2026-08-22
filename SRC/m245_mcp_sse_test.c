#include "llm_internal.h"

int command_line_complete(const char *input, size_t length, int eof)
{
    (void)input; (void)length; (void)eof; return 0;
}

int command_read_stream(FILE *stream, char *buffer, size_t buffer_size)
{
    (void)stream; (void)buffer; (void)buffer_size; return 0;
}

typedef struct m245_probe {
    int stdio_calls;
    int http_calls;
    int sse_calls;
    int succeed;
    char target[256];
    char tool[64];
} m245_probe;

static int fake_stdio(const char *target, const char *tool,
                      const char *arguments, char *result,
                      size_t result_size, void *context)
{
    m245_probe *probe = (m245_probe *)context;
    (void)arguments;
    ++probe->stdio_calls;
    (void)snprintf(probe->target, sizeof(probe->target), "%s", target);
    (void)snprintf(probe->tool, sizeof(probe->tool), "%s", tool);
    if (!probe->succeed) return 0;
    (void)snprintf(result, result_size, "stdio-ok\n");
    return 1;
}

static int fake_http(const char *target, const char *tool,
                     const char *arguments, char *result,
                     size_t result_size, void *context)
{
    m245_probe *probe = (m245_probe *)context;
    (void)arguments;
    ++probe->http_calls;
    (void)snprintf(probe->target, sizeof(probe->target), "%s", target);
    (void)snprintf(probe->tool, sizeof(probe->tool), "%s", tool);
    if (!probe->succeed) return 0;
    (void)snprintf(result, result_size, "http-ok\n");
    return 1;
}

static int fake_sse(const char *target, const char *tool,
                    const char *arguments, char *result,
                    size_t result_size, void *context)
{
    m245_probe *probe = (m245_probe *)context;
    ++probe->sse_calls;
    (void)snprintf(probe->target, sizeof(probe->target), "%s", target);
    (void)snprintf(probe->tool, sizeof(probe->tool), "%s", tool);
    if (!probe->succeed) return 0;
    (void)snprintf(result, result_size, "sse-ok args=%s\n", arguments);
    return 1;
}

static int require_true(int condition, const char *message)
{
    if (!condition) {
        (void)fprintf(stderr, "M245 MCP SSE regression failed: %s\n", message);
        return 0;
    }
    return 1;
}

int main(void)
{
    char output[4096];
    m245_probe probe;
    static char net_allow[] = "OVMS_AGENT_NET_ALLOW=tools.example.test";
    static char net_deny[] = "OVMS_AGENT_NET_DENY=";
    const char *config =
        "docs|stdio|@MCP_DOCS;"
        "issues|http|https://tools.example.test/mcp;"
        "stream|sse|https://tools.example.test/events;"
        "badsse|sse|file://not-sse";

    (void)putenv(net_allow);
    (void)putenv(net_deny);
    memset(&probe, 0, sizeof(probe));
    probe.succeed = 1;

    llm_test_reset_approval();
    if (!require_true(
            llm_mcp_exec_all_text(
                config, "stream watch alpha", fake_stdio, fake_http, fake_sse,
                &probe, output, sizeof(output)) &&
            strstr(output, "FULL approval policy is required") != NULL &&
            probe.sse_calls == 0,
            "approval refusal")) return 1;

    if (!require_true(llm_set_approval("full"), "enable full approval"))
        return 1;

    if (!require_true(
            llm_mcp_exec_all_text(
                config, "stream watch {\\\"topic\\\":\\\"build\\\"}",
                fake_stdio, fake_http, fake_sse, &probe,
                output, sizeof(output)) &&
            probe.sse_calls == 1 && probe.http_calls == 0 &&
            probe.stdio_calls == 0 &&
            strcmp(probe.target, "https://tools.example.test/events") == 0 &&
            strcmp(probe.tool, "watch") == 0 &&
            strstr(output, "Transport:  sse") != NULL &&
            strstr(output, "Status:     success") != NULL &&
            strstr(output, "sse-ok") != NULL,
            "approved SSE execution")) return 1;

    if (!require_true(
            llm_mcp_exec_all_text(
                config, "issues get 42", fake_stdio, fake_http, fake_sse,
                &probe, output, sizeof(output)) &&
            probe.http_calls == 1 && strstr(output, "Transport:  http") != NULL,
            "HTTP compatibility")) return 1;

    if (!require_true(
            llm_mcp_exec_all_text(
                config, "docs search RMS", fake_stdio, fake_http, fake_sse,
                &probe, output, sizeof(output)) &&
            probe.stdio_calls == 1 && strstr(output, "Transport:  stdio") != NULL,
            "stdio compatibility")) return 1;

    if (!require_true(
            llm_mcp_exec_all_text(
                config, "badsse watch x", fake_stdio, fake_http, fake_sse,
                &probe, output, sizeof(output)) &&
            strstr(output, "unsafe SSE endpoint") != NULL,
            "unsafe SSE refusal")) return 1;

    probe.succeed = 0;
    if (!require_true(
            llm_mcp_exec_all_text(
                config, "stream watch fail", fake_stdio, fake_http, fake_sse,
                &probe, output, sizeof(output)) &&
            strstr(output, "MCP sse execution failed") != NULL,
            "SSE failure normalization")) return 1;

    llm_test_reset_approval();
    (void)puts("M245 MCP SSE regression passed.");
    return 0;
}
