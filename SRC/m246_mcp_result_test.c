#include "llm_internal.h"

int command_line_complete(const char *input, size_t length, int eof)
{
    (void)input; (void)length; (void)eof; return 0;
}

int command_read_stream(FILE *stream, char *buffer, size_t buffer_size)
{
    (void)stream; (void)buffer; (void)buffer_size; return 0;
}

typedef struct m246_probe {
    int stdio_calls;
    int http_calls;
    int sse_calls;
    int succeed;
} m246_probe;

static int fake_stdio(const char *target, const char *tool,
                      const char *arguments, char *result,
                      size_t result_size, void *context)
{
    m246_probe *probe = (m246_probe *)context;
    (void)target; (void)tool; (void)arguments;
    ++probe->stdio_calls;
    if (!probe->succeed) return 0;
    (void)snprintf(result, result_size, "stdio-result\n");
    return 1;
}

static int fake_http(const char *target, const char *tool,
                     const char *arguments, char *result,
                     size_t result_size, void *context)
{
    m246_probe *probe = (m246_probe *)context;
    (void)target; (void)tool; (void)arguments;
    ++probe->http_calls;
    if (!probe->succeed) return 0;
    (void)snprintf(result, result_size, "http-result\n");
    return 1;
}

static int fake_sse(const char *target, const char *tool,
                    const char *arguments, char *result,
                    size_t result_size, void *context)
{
    m246_probe *probe = (m246_probe *)context;
    (void)target; (void)tool; (void)arguments;
    ++probe->sse_calls;
    if (!probe->succeed) return 0;
    (void)snprintf(result, result_size, "sse-result\n");
    return 1;
}

static int require_true(int condition, const char *message)
{
    if (!condition) {
        (void)fprintf(stderr, "M246 MCP result regression failed: %s\n", message);
        return 0;
    }
    return 1;
}

int main(void)
{
    const char *config =
        "docs|stdio|@MCP_DOCS;"
        "issues|http|https://tools.example.test/mcp;"
        "stream|sse|https://tools.example.test/events;"
        "bad|sse|file://bad";
    static char net_allow[] = "OVMS_AGENT_NET_ALLOW=tools.example.test";
    static char net_deny[] = "OVMS_AGENT_NET_DENY=";
    llm_mcp_result result;
    m246_probe probe;
    char output[4096];

    (void)putenv(net_allow);
    (void)putenv(net_deny);
    memset(&probe, 0, sizeof(probe));
    probe.succeed = 1;

    llm_test_reset_approval();
    if (!require_true(
            llm_mcp_run_result(config, "stream watch build",
                fake_stdio, fake_http, fake_sse, &probe, &result) &&
            result.status == LLM_MCP_RES_REFUSED &&
            strstr(result.detail, "FULL approval") != NULL &&
            probe.sse_calls == 0,
            "approval refusal result")) return 1;

    if (!require_true(llm_set_approval("full"), "enable full approval"))
        return 1;

    if (!require_true(
            llm_mcp_run_result(config, "docs search RMS",
                fake_stdio, fake_http, fake_sse, &probe, &result) &&
            result.status == LLM_MCP_RES_SUCCESS &&
            strcmp(result.transport, "stdio") == 0 &&
            strcmp(result.server, "docs") == 0 &&
            strcmp(result.tool, "search") == 0 &&
            strstr(result.detail, "stdio-result") != NULL &&
            probe.stdio_calls == 1,
            "stdio normalized success")) return 1;

    if (!require_true(
            llm_mcp_run_result(config, "issues get 42",
                fake_stdio, fake_http, fake_sse, &probe, &result) &&
            result.status == LLM_MCP_RES_SUCCESS &&
            strcmp(result.transport, "http") == 0 &&
            probe.http_calls == 1,
            "HTTP normalized success")) return 1;

    if (!require_true(
            llm_mcp_run_result(config, "stream watch x",
                fake_stdio, fake_http, fake_sse, &probe, &result) &&
            result.status == LLM_MCP_RES_SUCCESS &&
            strcmp(result.transport, "sse") == 0 &&
            probe.sse_calls == 1,
            "SSE normalized success")) return 1;

    if (!require_true(
            llm_mcp_result_text(&result, output, sizeof(output)) &&
            strstr(output, "Status:     success") != NULL &&
            strstr(output, "Transport:  sse") != NULL &&
            strstr(output, "sse-result") != NULL,
            "normalized formatter")) return 1;

    probe.succeed = 0;
    if (!require_true(
            llm_mcp_run_result(config, "issues get fail",
                fake_stdio, fake_http, fake_sse, &probe, &result) &&
            result.status == LLM_MCP_RES_FAILED &&
            strstr(result.detail, "execution failed") != NULL,
            "normalized failure")) return 1;

    probe.succeed = 1;
    if (!require_true(
            llm_mcp_run_result(config, "bad watch x",
                fake_stdio, fake_http, fake_sse, &probe, &result) &&
            result.status == LLM_MCP_RES_REFUSED &&
            strstr(result.detail, "unsafe SSE") != NULL,
            "normalized unsafe refusal")) return 1;

    if (!require_true(
            llm_mcp_run_result(config, "missing tool x",
                fake_stdio, fake_http, fake_sse, &probe, &result) &&
            result.status == LLM_MCP_RES_REFUSED &&
            strstr(result.detail, "not configured") != NULL,
            "unknown server refusal")) return 1;

    llm_test_reset_approval();
    (void)puts("M246 MCP normalized result regression passed.");
    return 0;
}
