#include "openai_internal.h"

/* Standalone regression link stubs. The M247 test links command_agent.c
   for the MCP command surface but does not exercise interactive input. */
int command_line_complete(const char *input,
    size_t input_size, int reached_eof)
{
    (void)input;
    (void)input_size;
    return reached_eof ? 1 : 0;
}

int command_read_stream(FILE *stream,
    char *input, size_t input_size)
{
    (void)stream;
    if (input != NULL && input_size > 0U) {
        input[0] = '\0';
    }
    return 0;
}

static int has_text(const char *text, const char *needle)
{
    return text != NULL && needle != NULL && strstr(text, needle) != NULL;
}

int main(void)
{
    openai_mcp_result result;
    char output[4096];
    char history[8192];
    char tools[4096];

    (void)remove("M247_MCP_TX.TMP");
    (void)remove("M247_SESS.TMP");
    (void)remove("M247_CUR.TMP");
    openai_test_tx_path("M247_MCP_TX.TMP");
    openai_test_session_paths("M247_SESS.TMP", "M247_CUR.TMP");

    memset(&result, 0, sizeof(result));
    result.status = OPENAI_MCP_RES_SUCCESS;
    (void)strcpy(result.server, "docs");
    (void)strcpy(result.transport, "stdio");
    (void)strcpy(result.tool, "lookup");
    (void)strcpy(result.detail, "found documentation");

    if (!openai_mcp_record_result(
            "docs lookup topic", &result, output, sizeof(output)) ||
        !has_text(output, "TOOL RESULT MCP") ||
        !has_text(output, "server=docs") ||
        !has_text(output, "status=success")) {
        (void)puts("M247 failed: normalized MCP feedback.");
        return 2;
    }

    if (!openai_session_results_text(
            "--------", history, sizeof(history)) ||
        !has_text(history, "tool=mcp_call") ||
        !has_text(history, "status=success") ||
        !has_text(history, "TOOL RESULT MCP")) {
        (void)puts("M247 failed: MCP session evidence.");
        return 2;
    }

    memset(&result, 0, sizeof(result));
    result.status = OPENAI_MCP_RES_REFUSED;
    (void)strcpy(result.server, "remote");
    (void)strcpy(result.transport, "http");
    (void)strcpy(result.tool, "write_issue");
    (void)strcpy(result.detail, "FULL approval policy is required.");

    if (!openai_mcp_record_result(
            "remote write_issue x", &result, output, sizeof(output)) ||
        !has_text(output, "status=refused")) {
        (void)puts("M247 failed: MCP refusal feedback.");
        return 2;
    }

    if (!openai_session_result_last(
            "--------", history, sizeof(history)) ||
        !has_text(history, "status=refused") ||
        !has_text(history, "server=remote")) {
        (void)puts("M247 failed: latest MCP evidence.");
        return 2;
    }

    if (!openai_tools_ext_text(tools, sizeof(tools)) ||
        !has_text(tools, "mcp_call") ||
        !has_text(tools, "approval=full")) {
        (void)puts("M247 failed: MCP tool discovery.");
        return 2;
    }

    (void)remove("M247_MCP_TX.TMP");
    (void)remove("M247_SESS.TMP");
    (void)remove("M247_CUR.TMP");
    openai_test_tx_path(NULL);
    openai_test_session_paths(NULL, NULL);

    (void)puts("M247 MCP agent feedback regression passed.");
    return 1;
}
