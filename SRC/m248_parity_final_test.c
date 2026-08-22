#include "llm_internal.h"
#include "command_internal.h"

/* Standalone regression link stubs. The full project option files pull in
   command handlers, but this test does not exercise interactive input. */
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
    char legacy[4096];
    char final_report[8192];
    char tools[4096];

    if (!llm_parity_text(legacy, sizeof(legacy)) ||
        !has_text(legacy, "MCP/tool servers:     not yet implemented") ||
        !has_text(legacy, "Unix sandbox parity:  not applicable on OpenVMS")) {
        (void)puts("M248 failed: legacy parity compatibility.");
        return 2;
    }

    if (!llm_final_parity_text(final_report, sizeof(final_report)) ||
        !has_text(final_report, "practical parity achieved") ||
        !has_text(final_report, "Fork and delegated branch:     adapted") ||
        !has_text(final_report, "MCP result/session evidence:   native") ||
        !has_text(final_report, "MCP protocol/auth lifecycle:   external bridge responsibility") ||
        !has_text(final_report, "Unix/Windows OS sandbox:       not applicable") ||
        !has_text(final_report, "legacy AGENT/PARITY remains frozen")) {
        (void)puts("M248 failed: final parity report.");
        return 2;
    }

    if (!llm_tools_ext_text(tools, sizeof(tools)) ||
        !has_text(tools, "mcp_call") ||
        !has_text(tools, "approval=full")) {
        (void)puts("M248 failed: extended tool catalog.");
        return 2;
    }

    if (command_find("AGENT/PARITY/FINAL") == NULL ||
        command_find("AGENT/TOOLS/EXT") == NULL) {
        (void)puts("M248 failed: live command registry.");
        return 2;
    }

    (void)puts("M248 final parity closure regression passed.");
    return 1;
}
