#include "llm_internal.h"

int command_line_complete(const char *input, size_t length, int eof)
{
    (void)input; (void)length; (void)eof; return 0;
}

int command_read_stream(FILE *stream, char *buffer, size_t buffer_size)
{
    (void)stream; (void)buffer; (void)buffer_size; return 0;
}

typedef struct transport_probe {
    int stdio_calls;
    int http_calls;
    int succeed;
    char target[256];
    char tool[64];
} transport_probe;

static int fake_stdio(const char *target, const char *tool,
                      const char *arguments, char *result,
                      size_t result_size, void *context)
{
    transport_probe *probe = (transport_probe *)context;
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
    transport_probe *probe = (transport_probe *)context;
    ++probe->http_calls;
    (void)snprintf(probe->target, sizeof(probe->target), "%s", target);
    (void)snprintf(probe->tool, sizeof(probe->tool), "%s", tool);
    if (!probe->succeed) return 0;
    (void)snprintf(result, result_size, "http-ok args=%s\n", arguments);
    return 1;
}

static int require_true(int condition, const char *message)
{
    if (!condition) {
        (void)fprintf(stderr, "M244 MCP HTTP regression failed: %s\n", message);
        return 0;
    }
    return 1;
}

int main(void)
{
    char output[4096];
    transport_probe probe;
    static char net_allow[] = "OVMS_AGENT_NET_ALLOW=tools.example.test";
    static char net_deny[] = "OVMS_AGENT_NET_DENY=";
    const char *config =
        "docs|stdio|@MCP_DOCS;"
        "issues|http|https://tools.example.test/mcp;"
        "stream|sse|https://tools.example.test/events;"
        "badhttp|http|file://not-http";

    (void)putenv(net_allow);
    (void)putenv(net_deny);
    memset(&probe, 0, sizeof(probe));
    probe.succeed = 1;

    openai_test_reset_approval();
    if (!require_true(
            openai_mcp_exec_transport_text(
                config, "issues get 42", fake_stdio, fake_http, &probe,
                output, sizeof(output)) &&
            strstr(output, "FULL approval policy is required") != NULL &&
            probe.http_calls == 0,
            "approval refusal")) return 1;

    if (!require_true(openai_set_approval("full"), "enable full approval"))
        return 1;

    if (!require_true(
            openai_mcp_exec_transport_text(
                config, "issues get {\\\"id\\\":42}", fake_stdio, fake_http,
                &probe, output, sizeof(output)) &&
            probe.http_calls == 1 && probe.stdio_calls == 0 &&
            strcmp(probe.target, "https://tools.example.test/mcp") == 0 &&
            strcmp(probe.tool, "get") == 0 &&
            strstr(output, "Transport:  http") != NULL &&
            strstr(output, "Status:     success") != NULL &&
            strstr(output, "http-ok") != NULL,
            "approved HTTP execution")) return 1;

    if (!require_true(
            openai_mcp_exec_transport_text(
                config, "docs search RMS", fake_stdio, fake_http, &probe,
                output, sizeof(output)) &&
            probe.stdio_calls == 1 &&
            strstr(output, "Transport:  stdio") != NULL,
            "stdio compatibility")) return 1;

    if (!require_true(
            openai_mcp_exec_transport_text(
                config, "stream watch x", fake_stdio, fake_http, &probe,
                output, sizeof(output)) &&
            strstr(output, "transport sse is not enabled in M244") != NULL,
            "SSE remains disabled")) return 1;

    if (!require_true(
            openai_mcp_exec_transport_text(
                config, "badhttp get x", fake_stdio, fake_http, &probe,
                output, sizeof(output)) &&
            strstr(output, "unsafe HTTP endpoint") != NULL,
            "unsafe endpoint refusal")) return 1;

    probe.succeed = 0;
    if (!require_true(
            openai_mcp_exec_transport_text(
                config, "issues get fail", fake_stdio, fake_http, &probe,
                output, sizeof(output)) &&
            strstr(output, "MCP http execution failed") != NULL,
            "HTTP failure normalization")) return 1;

    openai_test_reset_approval();
    (void)puts("M244 MCP HTTP regression passed.");
    return 0;
}
