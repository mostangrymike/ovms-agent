/* M258 Phase 7 network-policy wrapper around the mature parity/MCP module. */
#include "openai_internal.h"

int openai_net_check(const char *url, char *detail, size_t detail_size);

#define openai_mcp_exec_all_text openai_mcp_exec_all_base
#define openai_mcp_exec_transport_text openai_mcp_exec_transport_base
#define openai_mcp_run_result openai_mcp_run_result_base
#define openai_mcp_agent_call openai_mcp_agent_call_base
#define openai_show_mcp_execute openai_show_mcp_execute_base
#include "openai_parity.c"
#undef openai_mcp_exec_all_text
#undef openai_mcp_exec_transport_text
#undef openai_mcp_run_result
#undef openai_mcp_agent_call
#undef openai_show_mcp_execute

static int m258_mcp_net_gate(const char *config,
                             const char *arguments,
                             char *detail,
                             size_t detail_size)
{
    openai_mcp_server_desc servers[OPENAI_MCP_MAX_SERVERS];
    unsigned int count;
    unsigned int index;
    const char *cursor;
    char server[OPENAI_MCP_NAME_MAX];
    char tool[OPENAI_MCP_NAME_MAX];

    if (detail == NULL || detail_size == 0U) return 0;
    detail[0] = '\0';
    if (config == NULL || arguments == NULL) return 1;

    cursor = arguments;
    if (!openai_mcp_call_token(&cursor, server, sizeof(server)) ||
        !openai_mcp_call_token(&cursor, tool, sizeof(tool))) return 1;

    count = openai_mcp_parse(config, servers, NULL);
    for (index = 0U; index < count; ++index) {
        if (!openai_equal_ci(server, servers[index].name)) continue;
        if (openai_equal_ci(servers[index].transport, "http") ||
            openai_equal_ci(servers[index].transport, "sse")) {
            return openai_net_check(servers[index].target,
                                    detail, detail_size);
        }
        return 1;
    }
    return 1;
}

static int m258_net_refusal(char *output,
                            size_t output_size,
                            const char *detail)
{
    int written;

    if (output == NULL || output_size == 0U) return 0;
    written = snprintf(output, output_size,
        "MCP transport execution refused by network policy.\n%s\n",
        detail != NULL && *detail != '\0' ? detail : "Network policy denied request.");
    return written >= 0 && (size_t)written < output_size;
}

int openai_mcp_exec_all_text(const char *config,
                             const char *arguments,
                             openai_mcp_executor_fn stdio_executor,
                             openai_mcp_executor_fn http_executor,
                             openai_mcp_executor_fn sse_executor,
                             void *context,
                             char *output,
                             size_t output_size)
{
    char detail[512];

    if (!m258_mcp_net_gate(config, arguments, detail, sizeof(detail))) {
        return m258_net_refusal(output, output_size, detail);
    }
    return openai_mcp_exec_all_base(config, arguments,
        stdio_executor, http_executor, sse_executor,
        context, output, output_size);
}

int openai_mcp_exec_transport_text(const char *config,
                                   const char *arguments,
                                   openai_mcp_executor_fn stdio_executor,
                                   openai_mcp_executor_fn http_executor,
                                   void *context,
                                   char *output,
                                   size_t output_size)
{
    char detail[512];

    if (!m258_mcp_net_gate(config, arguments, detail, sizeof(detail))) {
        return m258_net_refusal(output, output_size, detail);
    }
    return openai_mcp_exec_transport_base(config, arguments,
        stdio_executor, http_executor, context, output, output_size);
}

int openai_mcp_run_result(const char *config,
                          const char *arguments,
                          openai_mcp_executor_fn stdio_executor,
                          openai_mcp_executor_fn http_executor,
                          openai_mcp_executor_fn sse_executor,
                          void *context,
                          openai_mcp_result *result)
{
    char detail[512];

    if (result == NULL) return 0;
    if (!m258_mcp_net_gate(config, arguments, detail, sizeof(detail))) {
        openai_mcp_res_clear(result);
        result->status = OPENAI_MCP_RES_REFUSED;
        openai_mcp_res_copy(result->detail, sizeof(result->detail), detail);
        return 1;
    }
    return openai_mcp_run_result_base(config, arguments,
        stdio_executor, http_executor, sse_executor, context, result);
}

int openai_mcp_agent_call(const char *arguments,
                          char *output, size_t output_size)
{
    openai_mcp_result result;

    if (arguments == NULL || output == NULL || output_size == 0U) return 0;
    openai_auto_note_tool();
    if (!openai_mcp_run_result(
            getenv("OVMS_AGENT_MCP_SERVERS"), arguments,
            openai_mcp_bridge_executor, openai_mcp_http_bridge_executor,
            openai_mcp_sse_bridge_executor, NULL, &result)) return 0;
    return openai_mcp_record_result(arguments, &result, output, output_size);
}

void openai_show_mcp_execute(const char *arguments)
{
    char output[4096];

    if (!openai_mcp_agent_call(arguments, output, sizeof(output))) {
        (void)puts(
            "Usage: AGENT/MCP/EXEC <server-name> <tool-name> [arguments]");
        return;
    }
    (void)fputs(output, stdout);
}
