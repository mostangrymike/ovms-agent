#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "llm_internal.h"

int command_line_complete(const char *input, size_t input_size, int reached_eof)
{
    (void)input; (void)input_size; (void)reached_eof; return 0;
}

int command_read_stream(FILE *stream, char *input, size_t input_size)
{
    (void)stream; (void)input; (void)input_size; return 0;
}

int main(void)
{
    agent_state state;
    char output[8192];

    (void)memset(&state, 0, sizeof(state));
    state.project_root = "SYS$SYSDEVICE:[MIKE.OVMS_AGENT]";
    state.api_key_defined = 1;
    state.write_enabled = 1;
    state.dcl_enabled = 0;

    llm_test_reset_approval();

    if (!llm_tools_text(output, sizeof(output)) ||
        strstr(output, "Registered parity tools: 9") == NULL ||
        strstr(output, "read_file") == NULL ||
        strstr(output, "run_build") == NULL ||
        strstr(output, "effect=write") == NULL) {
        (void)puts("M227 failed: tool catalog.");
        return EXIT_FAILURE;
    }

    if (!llm_tool_info_text("replace_text", output, sizeof(output)) ||
        strstr(output, "Effect:      write") == NULL ||
        strstr(output, "Approval:    workspace") == NULL ||
        !llm_tool_info_text("structured_patch", output, sizeof(output)) ||
        strstr(output, "Effect:      write") == NULL ||
        strstr(output, "Approval:    workspace") == NULL ||
        llm_tool_info_text("unknown_tool", output, sizeof(output))) {
        (void)puts("M227 failed: tool metadata.");
        return EXIT_FAILURE;
    }

    if (!llm_approval_text(output, sizeof(output)) ||
        strstr(output, "Policy: read-only") == NULL ||
        !llm_set_approval("workspace") ||
        !llm_approval_text(output, sizeof(output)) ||
        strstr(output, "Policy: workspace") == NULL ||
        strstr(output, "Source: session override") == NULL ||
        llm_set_approval("dangerous")) {
        (void)puts("M227 failed: approval policy.");
        return EXIT_FAILURE;
    }

    if (!llm_context_text(&state, output, sizeof(output)) ||
        strstr(output, "SYS$SYSDEVICE:[MIKE.OVMS_AGENT]") == NULL ||
        strstr(output, "Write gate:        enabled") == NULL ||
        strstr(output, "DCL gate:          disabled") == NULL ||
        strstr(output, "Approval policy:   workspace") == NULL ||
        strstr(output, "Parity tools:      9") == NULL) {
        (void)puts("M227 failed: execution context.");
        return EXIT_FAILURE;
    }

    if (!llm_parity_text(output, sizeof(output)) ||
        strstr(output, "Unified EXEC entry:   available") == NULL ||
        strstr(output, "Dry-run planning:     available") == NULL ||
        strstr(output, "MCP/tool servers:     not yet implemented") == NULL) {
        (void)puts("M227 failed: parity status.");
        return EXIT_FAILURE;
    }

    llm_test_reset_approval();

    if (!llm_approval_text(output, sizeof(output)) ||
        strstr(output, "Policy: read-only") == NULL) {
        (void)puts("M227 failed: approval reset.");
        return EXIT_FAILURE;
    }

    (void)puts("Codex parity foundation bundle test passed.");
    return EXIT_SUCCESS;
}
