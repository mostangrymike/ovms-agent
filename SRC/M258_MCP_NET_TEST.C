#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "openai_internal.h"

int openai_net_allow_once(const char *domain);
void openai_test_net_reset(void);

int command_line_complete(const char *input, size_t length, int eof)
{
    (void)input; (void)length; (void)eof; return 0;
}

int command_read_stream(FILE *stream, char *buffer, size_t buffer_size)
{
    (void)stream; (void)buffer; (void)buffer_size; return 0;
}

typedef struct m258_probe {
    int stdio_calls;
    int http_calls;
    int sse_calls;
} m258_probe;

static int fake_stdio(const char *target, const char *tool,
                      const char *arguments, char *result,
                      size_t result_size, void *context)
{
    m258_probe *probe = (m258_probe *)context;
    (void)target; (void)tool; (void)arguments;
    ++probe->stdio_calls;
    (void)snprintf(result, result_size, "stdio-ok\n");
    return 1;
}

static int fake_http(const char *target, const char *tool,
                     const char *arguments, char *result,
                     size_t result_size, void *context)
{
    m258_probe *probe = (m258_probe *)context;
    (void)target; (void)tool; (void)arguments;
    ++probe->http_calls;
    (void)snprintf(result, result_size, "http-ok\n");
    return 1;
}

static int fake_sse(const char *target, const char *tool,
                    const char *arguments, char *result,
                    size_t result_size, void *context)
{
    m258_probe *probe = (m258_probe *)context;
    (void)target; (void)tool; (void)arguments;
    ++probe->sse_calls;
    (void)snprintf(result, result_size, "sse-ok\n");
    return 1;
}

static int require_true(int condition, const char *message)
{
    if (!condition) {
        (void)fprintf(stderr, "M258 MCP network regression failed: %s\n", message);
        return 0;
    }
    return 1;
}

int main(void)
{
    const char *config =
        "docs|stdio|@MCP_DOCS;"
        "issues|http|https://tools.example.test/mcp;"
        "stream|sse|https://stream.example.test/events;";
    static char allow_none[] = "OVMS_AGENT_NET_ALLOW=";
    static char deny_none[] = "OVMS_AGENT_NET_DENY=";
    static char allow_http[] = "OVMS_AGENT_NET_ALLOW=tools.example.test";
    static char deny_http[] = "OVMS_AGENT_NET_DENY=tools.example.test";
    m258_probe probe;
    openai_mcp_result result;
    char output[4096];

    (void)putenv(allow_none);
    (void)putenv(deny_none);
    openai_test_net_reset();
    openai_test_reset_approval();
    if (!require_true(openai_set_approval("full"), "enable full approval"))
        return 1;
    memset(&probe, 0, sizeof(probe));

    if (!require_true(
            openai_mcp_exec_all_text(config, "issues get 42",
                fake_stdio, fake_http, fake_sse, &probe,
                output, sizeof(output)) &&
            strstr(output, "refused by network policy") != NULL &&
            strstr(output, "default deny") != NULL &&
            probe.http_calls == 0,
            "default-denied HTTP never executes")) return 1;

    if (!require_true(
            openai_mcp_run_result(config, "stream watch x",
                fake_stdio, fake_http, fake_sse, &probe, &result) &&
            result.status == OPENAI_MCP_RES_REFUSED &&
            strstr(result.detail, "default deny") != NULL &&
            probe.sse_calls == 0,
            "default-denied SSE normalized refusal")) return 1;

    (void)putenv(allow_http);
    if (!require_true(
            openai_mcp_exec_all_text(config, "issues get 42",
                fake_stdio, fake_http, fake_sse, &probe,
                output, sizeof(output)) &&
            strstr(output, "Status:     success") != NULL &&
            probe.http_calls == 1,
            "allow-listed HTTP executes")) return 1;

    (void)putenv(deny_http);
    if (!require_true(
            openai_mcp_exec_all_text(config, "issues get 42",
                fake_stdio, fake_http, fake_sse, &probe,
                output, sizeof(output)) &&
            strstr(output, "explicit deny") != NULL &&
            probe.http_calls == 1,
            "deny overrides allow before executor")) return 1;

    (void)putenv(allow_none);
    (void)putenv(deny_none);
    if (!require_true(openai_net_allow_once("stream.example.test"),
                      "create one-shot exception")) return 1;
    if (!require_true(
            openai_mcp_exec_all_text(config, "stream watch once",
                fake_stdio, fake_http, fake_sse, &probe,
                output, sizeof(output)) &&
            strstr(output, "Status:     success") != NULL &&
            probe.sse_calls == 1,
            "one-shot SSE exception executes")) return 1;
    if (!require_true(
            openai_mcp_exec_all_text(config, "stream watch twice",
                fake_stdio, fake_http, fake_sse, &probe,
                output, sizeof(output)) &&
            strstr(output, "default deny") != NULL &&
            probe.sse_calls == 1,
            "one-shot exception consumed")) return 1;

    if (!require_true(
            openai_mcp_exec_all_text(config, "docs search RMS",
                fake_stdio, fake_http, fake_sse, &probe,
                output, sizeof(output)) &&
            strstr(output, "Transport:  stdio") != NULL &&
            probe.stdio_calls == 1,
            "stdio unaffected by network policy")) return 1;

    openai_test_net_reset();
    openai_test_reset_approval();
    (void)putenv(allow_none);
    (void)putenv(deny_none);
    (void)puts("M258 MCP network-policy integration regression passed.");
    return 0;
}
