/* M258 Phase 7 network-policy/lifecycle wrapper around mature MCP support. */
#include "llm_internal.h"

int openai_net_check(const char *url, char *detail, size_t detail_size);
int openai_mcp_run_result_base(const char *config,
                               const char *arguments,
                               openai_mcp_executor_fn stdio_executor,
                               openai_mcp_executor_fn http_executor,
                               openai_mcp_executor_fn sse_executor,
                               void *context,
                               openai_mcp_result *result);

/* Keep the mature implementations available as base entry points while
   replacing only the production bridge executors and public M258 gates. */
#define openai_mcp_exec_all_text openai_mcp_exec_all_base
#define openai_mcp_exec_transport_text openai_mcp_exec_transport_base
#define openai_mcp_run_result openai_mcp_run_result_base
#define openai_mcp_agent_call openai_mcp_agent_call_base
#define openai_show_mcp_execute openai_show_mcp_execute_base
#define openai_mcp_file_bridge_execute openai_mcp_bridge_file_base
#define openai_mcp_bridge_executor openai_mcp_bridge_exec_base
#define openai_mcp_http_bridge_executor openai_mcp_http_exec_base
#define openai_mcp_sse_bridge_executor openai_mcp_sse_exec_base
#include "openai_parity.c"
#undef openai_mcp_exec_all_text
#undef openai_mcp_exec_transport_text
#undef openai_mcp_run_result
#undef openai_mcp_agent_call
#undef openai_show_mcp_execute
#undef openai_mcp_file_bridge_execute
#undef openai_mcp_bridge_executor
#undef openai_mcp_http_bridge_executor
#undef openai_mcp_sse_bridge_executor

#define M258_MCP_TIMEOUT_DEFAULT 30U
#define M258_MCP_TIMEOUT_MAX 3600U
#define M258_MCP_OUTPUT_DEFAULT 1536U
#define M258_MCP_OUTPUT_MIN 128U
#define M258_MCP_OUTPUT_MAX 1536U

static char m258_bridge_detail[256];

static unsigned int m258_env_uint(const char *name,
                                  unsigned int default_value,
                                  unsigned int minimum,
                                  unsigned int maximum)
{
    const char *value;
    const unsigned char *cursor;
    unsigned int result;
    unsigned int digit;

    value = getenv(name);
    if (value == NULL || *value == '\0') return default_value;

    result = 0U;
    cursor = (const unsigned char *)value;
    while (*cursor != (unsigned char)'\0') {
        if (*cursor < (unsigned char)'0' || *cursor > (unsigned char)'9')
            return default_value;
        digit = (unsigned int)(*cursor - (unsigned char)'0');
        if (result > (maximum - digit) / 10U) return default_value;
        result = result * 10U + digit;
        ++cursor;
    }
    if (result < minimum || result > maximum) return default_value;
    return result;
}

static unsigned int m258_mcp_timeout(void)
{
    return m258_env_uint("OVMS_AGENT_MCP_TIMEOUT_SECONDS",
                         M258_MCP_TIMEOUT_DEFAULT, 1U,
                         M258_MCP_TIMEOUT_MAX);
}

static unsigned int m258_mcp_output_limit(void)
{
    return m258_env_uint("OVMS_AGENT_MCP_OUTPUT_BYTES",
                         M258_MCP_OUTPUT_DEFAULT,
                         M258_MCP_OUTPUT_MIN,
                         M258_MCP_OUTPUT_MAX);
}

static int m258_bridge_note(char *result,
                            size_t result_size,
                            int status,
                            unsigned int timeout_seconds,
                            unsigned int output_limit,
                            const char *outcome)
{
    int written;

    if (result == NULL || result_size == 0U) return 0;
    written = snprintf(result, result_size,
        "condition_status=%08X\n"
        "timeout_seconds=%u\n"
        "output_limit=%u\n"
        "outcome=%s\n",
        (unsigned int)status, timeout_seconds, output_limit,
        outcome != NULL ? outcome : "unknown");
    return written >= 0 && (size_t)written < result_size;
}

static int m258_mcp_file_exec(const char *bridge,
                              const char *transport,
                              const char *target,
                              const char *tool,
                              const char *arguments,
                              char *result,
                              size_t result_size)
{
    FILE *file;
    char command[OPENAI_MCP_BRIDGE_COMMAND_MAX];
    char line[512];
    size_t used;
    size_t response_used;
    size_t length;
    unsigned int timeout_seconds;
    unsigned int output_limit;
    int written;
    int status;

    if (!openai_mcp_stdio_target_valid(bridge) || transport == NULL ||
        target == NULL || tool == NULL || arguments == NULL ||
        result == NULL || result_size == 0U) return 0;

    timeout_seconds = m258_mcp_timeout();
    output_limit = m258_mcp_output_limit();
    result[0] = '\0';
    m258_bridge_detail[0] = '\0';

    file = fopen(OPENAI_MCP_BRIDGE_REQUEST_FILE, "w");
    if (file == NULL) return 0;

    if (strcmp(transport, "stdio") == 0) {
        if (fprintf(file, "tool=%s\narguments=%s\n", tool, arguments) < 0) {
            (void)fclose(file);
            (void)remove(OPENAI_MCP_BRIDGE_REQUEST_FILE);
            return 0;
        }
    } else {
        if (fprintf(file,
                    "transport=%s\ntarget=%s\ntool=%s\narguments=%s\n",
                    transport, target, tool, arguments) < 0) {
            (void)fclose(file);
            (void)remove(OPENAI_MCP_BRIDGE_REQUEST_FILE);
            return 0;
        }
    }
    if (fprintf(file, "timeout_seconds=%u\noutput_limit=%u\n",
                timeout_seconds, output_limit) < 0 || fclose(file) != 0) {
        (void)remove(OPENAI_MCP_BRIDGE_REQUEST_FILE);
        return 0;
    }

    (void)remove(OPENAI_MCP_BRIDGE_RESPONSE_FILE);
    written = snprintf(command, sizeof(command), "%s %s %s", bridge,
                       OPENAI_MCP_BRIDGE_REQUEST_FILE,
                       OPENAI_MCP_BRIDGE_RESPONSE_FILE);
    if (written < 0 || (size_t)written >= sizeof(command)) {
        (void)remove(OPENAI_MCP_BRIDGE_REQUEST_FILE);
        return 0;
    }

    /* The bridge receives timeout_seconds as an explicit contract field.
       Core preserves exact OpenVMS condition status; bridge-side timeout
       enforcement is required because system() cannot safely kill a hung
       subprocess portably across supported OpenVMS releases. */
    status = system(command);
    if ((status & 1) == 0) {
        (void)m258_bridge_note(result, result_size, status,
                               timeout_seconds, output_limit,
                               "bridge_failed");
        (void)snprintf(m258_bridge_detail, sizeof(m258_bridge_detail),
                       "transport execution failed; condition_status=%08X",
                       (unsigned int)status);
        (void)remove(OPENAI_MCP_BRIDGE_REQUEST_FILE);
        (void)remove(OPENAI_MCP_BRIDGE_RESPONSE_FILE);
        return 0;
    }

    file = fopen(OPENAI_MCP_BRIDGE_RESPONSE_FILE, "r");
    if (file == NULL) {
        (void)m258_bridge_note(result, result_size, status,
                               timeout_seconds, output_limit,
                               "response_missing");
        (void)snprintf(m258_bridge_detail, sizeof(m258_bridge_detail),
                       "bridge response missing; condition_status=%08X",
                       (unsigned int)status);
        (void)remove(OPENAI_MCP_BRIDGE_REQUEST_FILE);
        return 0;
    }

    if (!m258_bridge_note(result, result_size, status,
                          timeout_seconds, output_limit, "success")) {
        (void)fclose(file);
        (void)remove(OPENAI_MCP_BRIDGE_REQUEST_FILE);
        (void)remove(OPENAI_MCP_BRIDGE_RESPONSE_FILE);
        return 0;
    }
    used = strlen(result);
    response_used = 0U;

    while (fgets(line, sizeof(line), file) != NULL) {
        length = strlen(line);
        if (length > (size_t)output_limit - response_used ||
            length >= result_size - used) {
            (void)fclose(file);
            (void)m258_bridge_note(result, result_size, status,
                                   timeout_seconds, output_limit,
                                   "output_limit_exceeded");
            (void)snprintf(m258_bridge_detail, sizeof(m258_bridge_detail),
                           "bridge output exceeded %u bytes; condition_status=%08X",
                           output_limit, (unsigned int)status);
            (void)remove(OPENAI_MCP_BRIDGE_REQUEST_FILE);
            (void)remove(OPENAI_MCP_BRIDGE_RESPONSE_FILE);
            return 0;
        }
        (void)memcpy(result + used, line, length);
        used += length;
        response_used += length;
        result[used] = '\0';
    }

    (void)fclose(file);
    (void)remove(OPENAI_MCP_BRIDGE_REQUEST_FILE);
    (void)remove(OPENAI_MCP_BRIDGE_RESPONSE_FILE);
    return 1;
}

static int m258_mcp_stdio_exec(const char *target,
                               const char *tool,
                               const char *arguments,
                               char *result,
                               size_t result_size,
                               void *context)
{
    (void)context;
    if (!openai_mcp_stdio_target_valid(target)) return 0;
    return m258_mcp_file_exec(target, "stdio", target, tool, arguments,
                              result, result_size);
}

static int m258_mcp_http_exec(const char *target,
                              const char *tool,
                              const char *arguments,
                              char *result,
                              size_t result_size,
                              void *context)
{
    const char *bridge;
    (void)context;
    if (!openai_mcp_http_target_valid(target)) return 0;
    bridge = getenv("OVMS_AGENT_MCP_HTTP_BRIDGE");
    if (!openai_mcp_stdio_target_valid(bridge)) return 0;
    return m258_mcp_file_exec(bridge, "http", target, tool, arguments,
                              result, result_size);
}

static int m258_mcp_sse_exec(const char *target,
                             const char *tool,
                             const char *arguments,
                             char *result,
                             size_t result_size,
                             void *context)
{
    const char *bridge;
    (void)context;
    if (!openai_mcp_http_target_valid(target)) return 0;
    bridge = getenv("OVMS_AGENT_MCP_SSE_BRIDGE");
    if (!openai_mcp_stdio_target_valid(bridge)) return 0;
    return m258_mcp_file_exec(bridge, "sse", target, tool, arguments,
                              result, result_size);
}

/* Focused OpenVMS regression hook; no production command exposes this. */
int openai_test_mcp_bridge(const char *bridge,
                           const char *transport,
                           const char *target,
                           char *result,
                           size_t result_size)
{
    return m258_mcp_file_exec(bridge, transport, target,
                              "probe", "test", result, result_size);
}

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

    /* Preserve mature M243-M246 refusal ordering: FULL approval and
       endpoint syntax are checked before M258 network authorization. */
    if (strcmp(openai_approval_name(), "full") != 0) return 1;

    cursor = arguments;
    if (!openai_mcp_call_token(&cursor, server, sizeof(server)) ||
        !openai_mcp_call_token(&cursor, tool, sizeof(tool))) return 1;

    count = openai_mcp_parse(config, servers, NULL);
    for (index = 0U; index < count; ++index) {
        if (!openai_equal_ci(server, servers[index].name)) continue;
        if (openai_equal_ci(servers[index].transport, "http") ||
            openai_equal_ci(servers[index].transport, "sse")) {
            if (!openai_mcp_http_target_valid(servers[index].target)) return 1;
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
        detail != NULL && *detail != '\0' ? detail :
        "Network policy denied request.");
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

    if (!m258_mcp_net_gate(config, arguments, detail, sizeof(detail)))
        return m258_net_refusal(output, output_size, detail);
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

    if (!m258_mcp_net_gate(config, arguments, detail, sizeof(detail)))
        return m258_net_refusal(output, output_size, detail);
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
    int ok;

    if (result == NULL) return 0;
    if (!m258_mcp_net_gate(config, arguments, detail, sizeof(detail))) {
        openai_mcp_res_clear(result);
        result->status = OPENAI_MCP_RES_REFUSED;
        openai_mcp_res_copy(result->detail, sizeof(result->detail), detail);
        return 1;
    }

    m258_bridge_detail[0] = '\0';
    ok = openai_mcp_run_result_base(config, arguments,
        stdio_executor, http_executor, sse_executor, context, result);
    if (ok && result->status == OPENAI_MCP_RES_FAILED &&
        m258_bridge_detail[0] != '\0') {
        openai_mcp_res_copy(result->detail, sizeof(result->detail),
                            m258_bridge_detail);
    }
    return ok;
}

int openai_mcp_agent_call(const char *arguments,
                          char *output, size_t output_size)
{
    openai_mcp_result result;
    char event[256];
    const char *status_name;

    if (arguments == NULL || output == NULL || output_size == 0U) return 0;
    openai_auto_note_tool();
    if (!openai_mcp_run_result(
            getenv("OVMS_AGENT_MCP_SERVERS"), arguments,
            m258_mcp_stdio_exec, m258_mcp_http_exec,
            m258_mcp_sse_exec, NULL, &result)) return 0;

    if (result.status == OPENAI_MCP_RES_SUCCESS) status_name = "success";
    else if (result.status == OPENAI_MCP_RES_FAILED) status_name = "failed";
    else status_name = "refused";
    (void)snprintf(event, sizeof(event),
        "server=%s transport=%s tool=%s result=%s",
        result.server, result.transport, result.tool, status_name);
    openai_log_event("MCP", event,
                     result.status == OPENAI_MCP_RES_SUCCESS ? 1 : 2);

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
