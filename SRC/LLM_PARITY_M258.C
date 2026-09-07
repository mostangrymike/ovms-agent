/* M267 neutral compatibility boundary for the mature M258 parity body. */
/* M268 overlays a scoped AUTOPILOT session policy without changing the
 * mature approval engine.  The base engine is deliberately kept at
 * WORKSPACE while AUTOPILOT is active, so every FULL gate remains closed.
 */
#include "SETTINGS.H"

#define llm_approval_name llm_approval_name_base
#define llm_approval_text llm_approval_text_base
#define llm_show_approval llm_show_approval_base
#define llm_set_approval llm_set_approval_base
#define llm_set_approval_cmd llm_set_approval_cmd_base
#define llm_reset_approval llm_reset_approval_base
#define llm_tools_text llm_tools_text_base
#define llm_tools_ext_text llm_tools_ext_text_base
#define llm_show_tools llm_show_tools_base
#define llm_show_tools_ext llm_show_tools_ext_base
#define llm_tool_info_text llm_tool_info_text_base
#define llm_show_tool_info llm_show_tool_info_base
#define llm_context_text llm_context_text_base
#define llm_show_context llm_show_context_base
#define llm_mcp_call_text llm_mcp_call_text_base
#define llm_show_mcp_call llm_show_mcp_call_base
#include "LLM_PARITY_M258_CORE.C"
#undef llm_approval_name
#undef llm_approval_text
#undef llm_show_approval
#undef llm_set_approval
#undef llm_set_approval_cmd
#undef llm_reset_approval
#undef llm_tools_text
#undef llm_tools_ext_text
#undef llm_show_tools
#undef llm_show_tools_ext
#undef llm_tool_info_text
#undef llm_show_tool_info
#undef llm_context_text
#undef llm_show_context
#undef llm_mcp_call_text
#undef llm_show_mcp_call

#define M289_BUILD_SOURCE_LINE \
    "  build_source     effect=execute approval=full+write+DCL\n"

static int m289_replace_nine(char *text, size_t text_size,
                             const char *marker)
{
    char *match;
    char *digit;
    size_t used;
    size_t tail;

    if (text == NULL || text_size == 0U || marker == NULL) {
        return 0;
    }

    match = strstr(text, marker);
    if (match == NULL) {
        return 0;
    }

    digit = match + strlen(marker) - 1U;
    if (*digit != '9') {
        return 0;
    }

    used = strlen(text);
    if (used + 1U >= text_size) {
        return 0;
    }

    tail = strlen(digit + 1) + 1U;
    memmove(digit + 2, digit + 1, tail);
    digit[0] = '1';
    digit[1] = '0';
    return 1;
}

static int m289_append_build_source(char *text, size_t text_size)
{
    size_t used;
    size_t extra;

    if (text == NULL || text_size == 0U) {
        return 0;
    }

    used = strlen(text);
    extra = strlen(M289_BUILD_SOURCE_LINE);
    if (extra >= text_size - used) {
        return 0;
    }

    memcpy(text + used, M289_BUILD_SOURCE_LINE, extra + 1U);
    return 1;
}

static int m289_insert_build_source(char *text, size_t text_size,
                                    const char *marker)
{
    char *position;
    size_t used;
    size_t extra;
    size_t tail;

    if (text == NULL || text_size == 0U || marker == NULL) {
        return 0;
    }

    position = strstr(text, marker);
    if (position == NULL) {
        return 0;
    }

    used = strlen(text);
    extra = strlen(M289_BUILD_SOURCE_LINE);
    if (extra >= text_size - used) {
        return 0;
    }

    tail = strlen(position) + 1U;
    memmove(position + extra, position, tail);
    memcpy(position, M289_BUILD_SOURCE_LINE, extra);
    return 1;
}

int llm_tools_text(char *output, size_t output_size)
{
    if (!llm_tools_text_base(output, output_size) ||
        !m289_replace_nine(output, output_size,
                           "Registered parity tools: 9") ||
        !m289_append_build_source(output, output_size)) {
        return 0;
    }

    return 1;
}

void llm_show_tools(void)
{
    char output[4096];

    if (!llm_tools_text(output, sizeof(output))) {
        (void)puts("Unable to build tool catalog.");
        return;
    }

    (void)fputs(output, stdout);
}

int llm_tools_ext_text(char *output, size_t output_size)
{
    if (!llm_tools_ext_text_base(output, output_size) ||
        !m289_replace_nine(output, output_size,
                           "Registered parity tools: 9") ||
        !m289_insert_build_source(output, output_size,
                                  "M247 external tools:")) {
        return 0;
    }

    return 1;
}

void llm_show_tools_ext(void)
{
    char output[4096];

    if (!llm_tools_ext_text(output, sizeof(output))) {
        (void)puts("Unable to build extended tool catalog.");
        return;
    }

    (void)fputs(output, stdout);
}

static int m306_tool_info_table(const char *arguments,
                                const llm_tool_desc *tools,
                                unsigned int count,
                                char *output,
                                size_t output_size)
{
    unsigned int index;
    int written;

    if (arguments == NULL || tools == NULL ||
        output == NULL || output_size == 0U) {
        return 0;
    }

    for (index = 0U; index < count; ++index) {
        if (!llm_equal_ci(arguments, tools[index].name)) {
            continue;
        }

        written = snprintf(
            output, output_size,
            "OVMS Agent tool information\n"
            "---------------------------\n"
            "Name:        %s\n"
            "Effect:      %s\n"
            "Approval:    %s\n"
            "Description: %s\n",
            tools[index].name,
            tools[index].effect,
            tools[index].approval,
            tools[index].description
        );
        return written >= 0 && (size_t)written < output_size;
    }

    return 0;
}

int llm_tool_info_text(const char *arguments,
                       char *output,
                       size_t output_size)
{
    int written;
    unsigned int count;

    if (arguments != NULL && llm_equal_ci(arguments, "build_source")) {
        if (output == NULL || output_size == 0U) {
            return 0;
        }

        written = snprintf(
            output, output_size,
            "OVMS Agent tool information\n"
            "---------------------------\n"
            "Name:        build_source\n"
            "Effect:      execute\n"
            "Approval:    full + write + DCL\n"
            "Description: Build one native source file through a validated language profile.\n"
        );
        return written >= 0 && (size_t)written < output_size;
    }

    count = (unsigned int)(sizeof(llm_m247_tools) /
                           sizeof(llm_m247_tools[0]));
    if (m306_tool_info_table(arguments, llm_m247_tools, count,
                             output, output_size)) {
        return 1;
    }

    count = (unsigned int)(sizeof(llm_m250_tools) /
                           sizeof(llm_m250_tools[0]));
    if (m306_tool_info_table(arguments, llm_m250_tools, count,
                             output, output_size)) {
        return 1;
    }

    return llm_tool_info_text_base(arguments, output, output_size);
}

void llm_show_tool_info(const char *arguments)
{
    char output[2048];

    if (!llm_tool_info_text(arguments, output, sizeof(output))) {
        (void)puts("Usage: AGENT/TOOLS/INFO <tool-name>");
        return;
    }

    (void)fputs(output, stdout);
}

static int m306_mcp_unknown_call(const char *config,
                                 const char *arguments,
                                 char *output,
                                 size_t output_size)
{
    llm_mcp_server_desc servers[LLM_MCP_MAX_SERVERS];
    unsigned int count;
    unsigned int index;
    const char *cursor;
    const char *payload;
    char server[LLM_MCP_NAME_MAX];
    char tool[LLM_MCP_NAME_MAX];
    int written;

    if (arguments == NULL || output == NULL || output_size == 0U) {
        return 0;
    }

    cursor = arguments;
    if (!llm_mcp_call_token(&cursor, server, sizeof(server)) ||
        !llm_mcp_call_token(&cursor, tool, sizeof(tool)) ||
        !llm_mcp_name_valid(server) || !llm_mcp_name_valid(tool)) {
        return 0;
    }

    payload = llm_skip_ws(cursor);
    if (!llm_mcp_call_args_valid(payload)) {
        return 0;
    }

    count = llm_mcp_parse(config, servers, NULL);
    for (index = 0U; index < count; ++index) {
        if (llm_equal_ci(server, servers[index].name)) {
            return 0;
        }
    }

    written = snprintf(output, output_size,
                       "MCP server is not configured: %s\n", server);
    return written >= 0 && (size_t)written < output_size;
}

int llm_mcp_call_text(const char *config,
                      const char *arguments,
                      char *output,
                      size_t output_size)
{
    if (llm_mcp_call_text_base(config, arguments, output, output_size)) {
        return 1;
    }

    return m306_mcp_unknown_call(config, arguments, output, output_size);
}

void llm_show_mcp_call(const char *arguments)
{
    char output[4096];

    if (!llm_mcp_call_text(getenv("OVMS_AGENT_MCP_SERVERS"),
                           arguments, output, sizeof(output))) {
        (void)puts(
            "Usage: AGENT/MCP/CALL <server-name> <tool-name> [arguments]");
        return;
    }

    (void)fputs(output, stdout);
}

int llm_context_text(const agent_state *state,
                     char *output,
                     size_t output_size)
{
    if (!llm_context_text_base(state, output, output_size) ||
        !m289_replace_nine(output, output_size,
                           "Parity tools:      9")) {
        return 0;
    }

    return 1;
}

void llm_show_context(const agent_state *state)
{
    char output[4096];

    if (!llm_context_text(state, output, sizeof(output))) {
        (void)puts("Unable to build agent context.");
        return;
    }

    (void)fputs(output, stdout);
}

static int m280_approval_base_default(void)
{
    char output[1024];

    if (!llm_approval_text_base(output, sizeof(output))) {
        return 0;
    }

    return strstr(output, "\nSource: default\n") != NULL;
}

static const char *m280_saved_approval(void)
{
    const char *value;

    if (!m280_approval_base_default() ||
        !settings_is_saved("approval_policy")) {
        return NULL;
    }

    value = settings_get("approval_policy");
    if (value == NULL) {
        return NULL;
    }

    if (strcmp(value, "read-only") == 0 ||
        strcmp(value, "workspace") == 0 ||
        strcmp(value, "full") == 0) {
        return value;
    }

    return NULL;
}

static const char *m280_approval_name_base(void)
{
    const char *saved;

    saved = m280_saved_approval();
    return saved != NULL ? saved : llm_approval_name_base();
}

static int m280_approval_text_base(char *output, size_t output_size)
{
    const char *saved;
    int written;

    saved = m280_saved_approval();
    if (saved == NULL) {
        return llm_approval_text_base(output, output_size);
    }

    if (output == NULL || output_size == 0U) {
        return 0;
    }

    written = snprintf(
        output, output_size,
        "OVMS Agent approval policy\n"
        "--------------------------\n"
        "Policy: %s\n"
        "Source: saved settings\n"
        "Environment: OVMS_AGENT_APPROVAL_POLICY\n"
        "Policies: read-only, workspace, full\n"
        "Note: write and DCL gates remain authoritative.\n",
        saved
    );

    return written >= 0 && (size_t)written < output_size;
}

#include "LLM_CREATE_CONTEXT_M277.INC"
#include "LLM_AUTOPILOT_POLICY.INC"
