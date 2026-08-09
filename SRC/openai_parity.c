#include "openai_internal.h"

#define OPENAI_APPROVAL_READ 0
#define OPENAI_APPROVAL_WORK 1
#define OPENAI_APPROVAL_FULL 2

typedef struct openai_tool_desc {
    const char *name;
    const char *effect;
    const char *approval;
    const char *description;
} openai_tool_desc;

static const openai_tool_desc openai_parity_tools[] = {
    { "read_file", "read", "none", "Read one project text file." },
    { "read_file_range", "read", "none", "Read a bounded line range." },
    { "list_directory", "read", "none", "List one project directory." },
    { "search_file", "read", "none", "Search project text for a pattern." },
    { "create_file", "write", "workspace", "Create one guarded project file." },
    { "replace_text", "write", "workspace", "Replace exact guarded project text." },
    { "replace_lines", "write", "workspace", "Replace a guarded line range." },
    { "structured_patch", "write", "workspace", "Apply multiple prevalidated non-overlapping hunks in one file." },
    { "run_build", "execute", "DCL gate", "Run the controlled project build." }
};

static const openai_tool_desc openai_m247_tools[] = {
    { "mcp_call", "external", "full", "Call one configured MCP server tool through the guarded transport layer." }
};

static const openai_tool_desc openai_m250_tools[] = {
    { "github_git", "external", "mixed", "Use guarded OpenVMS Git transport for GitHub repositories." },
    { "github_service", "external", "full", "Use the configured GitHub bridge for issues and pull requests." }
};

static int openai_approval_override = -1;

static int openai_equal_ci(const char *left, const char *right)
{
    unsigned char a;
    unsigned char b;

    if (left == NULL || right == NULL) {
        return 0;
    }

    while (*left != '\0' && *right != '\0') {
        a = (unsigned char)*left;
        b = (unsigned char)*right;

        if (a >= (unsigned char)'a' && a <= (unsigned char)'z') {
            a = (unsigned char)(a - (unsigned char)'a' + (unsigned char)'A');
        }
        if (b >= (unsigned char)'a' && b <= (unsigned char)'z') {
            b = (unsigned char)(b - (unsigned char)'a' + (unsigned char)'A');
        }

        if (a != b) {
            return 0;
        }

        ++left;
        ++right;
    }

    return *left == '\0' && *right == '\0';
}

static const char *openai_skip_ws(const char *text)
{
    if (text == NULL) {
        return "";
    }

    while (*text == ' ' || *text == '\t') {
        ++text;
    }

    return text;
}

static int openai_policy_value(const char *text)
{
    if (text == NULL || *text == '\0') {
        return -1;
    }

    if (openai_equal_ci(text, "READ-ONLY") ||
        openai_equal_ci(text, "READ_ONLY") ||
        openai_equal_ci(text, "READ")) {
        return OPENAI_APPROVAL_READ;
    }

    if (openai_equal_ci(text, "WORKSPACE") ||
        openai_equal_ci(text, "WRITE")) {
        return OPENAI_APPROVAL_WORK;
    }

    if (openai_equal_ci(text, "FULL")) {
        return OPENAI_APPROVAL_FULL;
    }

    return -1;
}

static const char *openai_policy_name(int policy)
{
    if (policy == OPENAI_APPROVAL_WORK) {
        return "workspace";
    }
    if (policy == OPENAI_APPROVAL_FULL) {
        return "full";
    }
    return "read-only";
}

static int openai_policy_source(void)
{
    const char *value;
    int parsed;

    if (openai_approval_override >= 0) {
        return openai_approval_override;
    }

    value = getenv("OVMS_AGENT_APPROVAL_POLICY");
    parsed = openai_policy_value(value);

    if (parsed >= 0) {
        return parsed;
    }

    return OPENAI_APPROVAL_READ;
}

static const char *openai_policy_origin(void)
{
    if (openai_approval_override >= 0) {
        return "session override";
    }

    if (openai_policy_value(
            getenv("OVMS_AGENT_APPROVAL_POLICY")) >= 0) {
        return "environment";
    }

    return "default";
}

int openai_tools_text(char *output, size_t output_size)
{
    size_t used;
    unsigned int index;
    unsigned int count;
    int written;

    if (output == NULL || output_size == 0U) {
        return 0;
    }

    count = (unsigned int)(sizeof(openai_parity_tools) /
                           sizeof(openai_parity_tools[0]));

    written = snprintf(
        output, output_size,
        "OVMS Agent tool catalog\n"
        "-----------------------\n"
        "Registered parity tools: %u\n",
        count
    );

    if (written < 0 || (size_t)written >= output_size) {
        return 0;
    }

    used = (size_t)written;

    for (index = 0U; index < count; ++index) {
        written = snprintf(
            output + used, output_size - used,
            "  %-16s effect=%-7s approval=%s\n",
            openai_parity_tools[index].name,
            openai_parity_tools[index].effect,
            openai_parity_tools[index].approval
        );

        if (written < 0 ||
            (size_t)written >= output_size - used) {
            return 0;
        }

        used += (size_t)written;
    }

    return 1;
}

void openai_show_tools(void)
{
    char output[4096];

    if (!openai_tools_text(output, sizeof(output))) {
        (void)puts("Unable to build tool catalog.");
        return;
    }

    (void)fputs(output, stdout);
}

int openai_tools_ext_text(char *output, size_t output_size)
{
    size_t used;
    unsigned int index;
    unsigned int count;
    int written;

    if (output == NULL || output_size == 0U) {
        return 0;
    }

    if (!openai_tools_text(output, output_size)) {
        return 0;
    }

    used = strlen(output);
    count = (unsigned int)(sizeof(openai_m247_tools) /
                           sizeof(openai_m247_tools[0]));

    written = snprintf(
        output + used, output_size - used,
        "M247 external tools: %u\n", count);
    if (written < 0 || (size_t)written >= output_size - used) {
        return 0;
    }
    used += (size_t)written;

    for (index = 0U; index < count; ++index) {
        written = snprintf(
            output + used, output_size - used,
            "  %-16s effect=%-7s approval=%s\n",
            openai_m247_tools[index].name,
            openai_m247_tools[index].effect,
            openai_m247_tools[index].approval);
        if (written < 0 || (size_t)written >= output_size - used) {
            return 0;
        }
        used += (size_t)written;
    }

    count = (unsigned int)(sizeof(openai_m250_tools) /
                           sizeof(openai_m250_tools[0]));
    written = snprintf(output + used, output_size - used,
                       "M250 GitHub tools: %u\n", count);
    if (written < 0 || (size_t)written >= output_size - used) {
        return 0;
    }
    used += (size_t)written;
    for (index = 0U; index < count; ++index) {
        written = snprintf(
            output + used, output_size - used,
            "  %-16s effect=%-7s approval=%s\n",
            openai_m250_tools[index].name,
            openai_m250_tools[index].effect,
            openai_m250_tools[index].approval);
        if (written < 0 || (size_t)written >= output_size - used) {
            return 0;
        }
        used += (size_t)written;
    }

    return 1;
}

void openai_show_tools_ext(void)
{
    char output[4096];

    if (!openai_tools_ext_text(output, sizeof(output))) {
        (void)puts("Unable to build extended tool catalog.");
        return;
    }

    (void)fputs(output, stdout);
}

int openai_tool_info_text(const char *arguments,
                          char *output,
                          size_t output_size)
{
    const char *name;
    unsigned int index;
    unsigned int count;
    int written;

    if (output == NULL || output_size == 0U) {
        return 0;
    }

    name = openai_skip_ws(arguments);

    if (*name == '\0' ||
        strchr(name, ' ') != NULL ||
        strchr(name, '\t') != NULL) {
        return 0;
    }

    count = (unsigned int)(sizeof(openai_parity_tools) /
                           sizeof(openai_parity_tools[0]));

    for (index = 0U; index < count; ++index) {
        if (!openai_equal_ci(name, openai_parity_tools[index].name)) {
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
            openai_parity_tools[index].name,
            openai_parity_tools[index].effect,
            openai_parity_tools[index].approval,
            openai_parity_tools[index].description
        );

        return written >= 0 && (size_t)written < output_size;
    }

    return 0;
}

void openai_show_tool_info(const char *arguments)
{
    char output[2048];

    if (!openai_tool_info_text(arguments, output, sizeof(output))) {
        (void)puts("Usage: AGENT/TOOLS/INFO <tool-name>");
        return;
    }

    (void)fputs(output, stdout);
}

int openai_approval_text(char *output, size_t output_size)
{
    int written;

    if (output == NULL || output_size == 0U) {
        return 0;
    }

    written = snprintf(
        output, output_size,
        "OVMS Agent approval policy\n"
        "--------------------------\n"
        "Policy: %s\n"
        "Source: %s\n"
        "Environment: OVMS_AGENT_APPROVAL_POLICY\n"
        "Policies: read-only, workspace, full\n"
        "Note: write and DCL gates remain authoritative.\n",
        openai_policy_name(openai_policy_source()),
        openai_policy_origin()
    );

    return written >= 0 && (size_t)written < output_size;
}

const char *openai_approval_name(void)
{
    return openai_policy_name(openai_policy_source());
}

void openai_show_approval(void)
{
    char output[2048];

    if (!openai_approval_text(output, sizeof(output))) {
        (void)puts("Unable to show approval policy.");
        return;
    }

    (void)fputs(output, stdout);
}

int openai_set_approval(const char *arguments)
{
    const char *value;
    int policy;

    value = openai_skip_ws(arguments);

    if (*value == '\0' ||
        strchr(value, ' ') != NULL ||
        strchr(value, '\t') != NULL) {
        return 0;
    }

    policy = openai_policy_value(value);

    if (policy < 0) {
        return 0;
    }

    openai_approval_override = policy;
    return 1;
}

void openai_set_approval_cmd(const char *arguments)
{
    if (!openai_set_approval(arguments)) {
        (void)puts(
            "Usage: AGENT/APPROVAL/SET <read-only|workspace|full>"
        );
        return;
    }

    (void)printf(
        "Session approval policy set to %s.\n",
        openai_policy_name(openai_policy_source())
    );
}

void openai_reset_approval(void)
{
    openai_approval_override = -1;
    (void)puts("Session approval policy reset.");
}

int openai_context_text(const agent_state *state,
                        char *output,
                        size_t output_size)
{
    unsigned int tool_count;
    int written;

    if (state == NULL || output == NULL || output_size == 0U) {
        return 0;
    }

    tool_count = (unsigned int)(sizeof(openai_parity_tools) /
                                sizeof(openai_parity_tools[0]));

    written = snprintf(
        output, output_size,
        "OVMS Agent execution context\n"
        "----------------------------\n"
        "Project root:      %s\n"
        "API key:           %s\n"
        "Write gate:        %s\n"
        "DCL gate:          %s\n"
        "Approval policy:   %s\n"
        "Parity tools:      %u\n"
        "Agent turn limit:  %d\n"
        "Plan turn limit:   %d\n",
        state->project_root != NULL ? state->project_root : "(not set)",
        state->api_key_defined ? "available" : "missing",
        state->write_enabled ? "enabled" : "disabled",
        state->dcl_enabled ? "enabled" : "disabled",
        openai_policy_name(openai_policy_source()),
        tool_count,
        OPENAI_AGENT_MAX_TURNS,
        OPENAI_PLAN_MAX_TURNS
    );

    return written >= 0 && (size_t)written < output_size;
}

void openai_show_context(const agent_state *state)
{
    char output[4096];

    if (!openai_context_text(state, output, sizeof(output))) {
        (void)puts("Unable to build agent context.");
        return;
    }

    (void)fputs(output, stdout);
}


#define OPENAI_MCP_MAX_SERVERS 8U
#define OPENAI_MCP_ENTRY_MAX 512U
#define OPENAI_MCP_NAME_MAX 64U
#define OPENAI_MCP_TRANSPORT_MAX 16U
#define OPENAI_MCP_TARGET_MAX 384U

typedef struct openai_mcp_server_desc {
    char name[OPENAI_MCP_NAME_MAX];
    char transport[OPENAI_MCP_TRANSPORT_MAX];
    char target[OPENAI_MCP_TARGET_MAX];
} openai_mcp_server_desc;

static int openai_mcp_name_valid(const char *name)
{
    const unsigned char *cursor;

    if (name == NULL || *name == '\0') {
        return 0;
    }

    cursor = (const unsigned char *)name;
    while (*cursor != '\0') {
        if (!(isalnum(*cursor) || *cursor == '_' ||
              *cursor == '-' || *cursor == '.')) {
            return 0;
        }
        ++cursor;
    }

    return 1;
}

static int openai_mcp_transport_valid(const char *transport)
{
    return openai_equal_ci(transport, "stdio") ||
           openai_equal_ci(transport, "http") ||
           openai_equal_ci(transport, "sse");
}

static char *openai_mcp_trim(char *text)
{
    char *end;

    if (text == NULL) {
        return text;
    }

    while (*text == ' ' || *text == '\t') {
        ++text;
    }

    end = text + strlen(text);
    while (end > text &&
           (end[-1] == ' ' || end[-1] == '\t')) {
        --end;
    }
    *end = '\0';
    return text;
}

static unsigned int openai_mcp_parse(
    const char *config,
    openai_mcp_server_desc servers[OPENAI_MCP_MAX_SERVERS],
    unsigned int *invalid_count)
{
    const char *cursor;
    const char *end;
    char entry[OPENAI_MCP_ENTRY_MAX];
    char *name;
    char *transport;
    char *target;
    char *first;
    char *second;
    size_t length;
    unsigned int count;
    unsigned int invalid;

    count = 0U;
    invalid = 0U;

    if (config == NULL) {
        config = "";
    }

    cursor = config;
    while (*cursor != '\0') {
        while (*cursor == ';' || *cursor == ' ' || *cursor == '\t') {
            ++cursor;
        }
        if (*cursor == '\0') {
            break;
        }

        end = strchr(cursor, ';');
        if (end == NULL) {
            end = cursor + strlen(cursor);
        }

        length = (size_t)(end - cursor);
        if (length == 0U || length >= sizeof(entry)) {
            ++invalid;
        } else {
            memcpy(entry, cursor, length);
            entry[length] = '\0';

            first = strchr(entry, '|');
            second = first != NULL ? strchr(first + 1, '|') : NULL;
            if (first == NULL || second == NULL ||
                strchr(second + 1, '|') != NULL) {
                ++invalid;
            } else {
                *first = '\0';
                *second = '\0';
                name = openai_mcp_trim(entry);
                transport = openai_mcp_trim(first + 1);
                target = openai_mcp_trim(second + 1);

                if (!openai_mcp_name_valid(name) ||
                    !openai_mcp_transport_valid(transport) ||
                    *target == '\0' ||
                    strlen(name) >= OPENAI_MCP_NAME_MAX ||
                    strlen(transport) >= OPENAI_MCP_TRANSPORT_MAX ||
                    strlen(target) >= OPENAI_MCP_TARGET_MAX) {
                    ++invalid;
                } else if (count >= OPENAI_MCP_MAX_SERVERS) {
                    ++invalid;
                } else {
                    strcpy(servers[count].name, name);
                    strcpy(servers[count].transport, transport);
                    strcpy(servers[count].target, target);
                    ++count;
                }
            }
        }

        cursor = *end == ';' ? end + 1 : end;
    }

    if (invalid_count != NULL) {
        *invalid_count = invalid;
    }
    return count;
}

int openai_mcp_catalog_text(const char *config,
                            char *output,
                            size_t output_size)
{
    openai_mcp_server_desc servers[OPENAI_MCP_MAX_SERVERS];
    unsigned int count;
    unsigned int invalid;
    unsigned int index;
    size_t used;
    int written;

    if (output == NULL || output_size == 0U) {
        return 0;
    }

    count = openai_mcp_parse(config, servers, &invalid);
    written = snprintf(
        output, output_size,
        "OVMS Agent MCP server catalog\n"
        "-----------------------------\n"
        "Configuration: OVMS_AGENT_MCP_SERVERS\n"
        "Format: name|transport|target;...\n"
        "Configured servers: %u\n"
        "Ignored entries:    %u\n",
        count, invalid
    );
    if (written < 0 || (size_t)written >= output_size) {
        return 0;
    }
    used = (size_t)written;

    for (index = 0U; index < count; ++index) {
        written = snprintf(
            output + used, output_size - used,
            "  %-16s transport=%-5s target=%s\n",
            servers[index].name,
            servers[index].transport,
            servers[index].target
        );
        if (written < 0 ||
            (size_t)written >= output_size - used) {
            return 0;
        }
        used += (size_t)written;
    }

    if (count == 0U) {
        written = snprintf(
            output + used, output_size - used,
            "  (none configured)\n"
        );
        if (written < 0 ||
            (size_t)written >= output_size - used) {
            return 0;
        }
    }

    return 1;
}

int openai_mcp_info_text(const char *config,
                         const char *arguments,
                         char *output,
                         size_t output_size)
{
    openai_mcp_server_desc servers[OPENAI_MCP_MAX_SERVERS];
    unsigned int count;
    unsigned int index;
    const char *name;
    int written;

    if (output == NULL || output_size == 0U) {
        return 0;
    }

    name = openai_skip_ws(arguments);
    if (*name == '\0' || strchr(name, ' ') != NULL ||
        strchr(name, '\t') != NULL) {
        return 0;
    }

    count = openai_mcp_parse(config, servers, NULL);
    for (index = 0U; index < count; ++index) {
        if (!openai_equal_ci(name, servers[index].name)) {
            continue;
        }

        written = snprintf(
            output, output_size,
            "OVMS Agent MCP server information\n"
            "---------------------------------\n"
            "Name:      %s\n"
            "Transport: %s\n"
            "Target:    %s\n"
            "State:     configured (transport execution pending)\n",
            servers[index].name,
            servers[index].transport,
            servers[index].target
        );
        return written >= 0 && (size_t)written < output_size;
    }

    return 0;
}

#define OPENAI_MCP_CALL_ARGS_MAX 1024U

static int openai_mcp_call_token(const char **cursor,
                                 char *output,
                                 size_t output_size)
{
    const char *start;
    size_t length;

    if (cursor == NULL || *cursor == NULL ||
        output == NULL || output_size == 0U) {
        return 0;
    }

    *cursor = openai_skip_ws(*cursor);
    start = *cursor;
    while (**cursor != '\0' &&
           **cursor != ' ' && **cursor != '\t') {
        ++(*cursor);
    }

    length = (size_t)(*cursor - start);
    if (length == 0U || length >= output_size) {
        return 0;
    }

    memcpy(output, start, length);
    output[length] = '\0';
    return 1;
}

static int openai_mcp_call_args_valid(const char *text)
{
    const unsigned char *cursor;

    if (text == NULL) {
        return 0;
    }

    cursor = (const unsigned char *)text;
    while (*cursor != '\0') {
        if (*cursor < 32U && *cursor != '\t') {
            return 0;
        }
        ++cursor;
    }
    return strlen(text) < OPENAI_MCP_CALL_ARGS_MAX;
}

int openai_mcp_call_text(const char *config,
                         const char *arguments,
                         char *output,
                         size_t output_size)
{
    openai_mcp_server_desc servers[OPENAI_MCP_MAX_SERVERS];
    unsigned int count;
    unsigned int index;
    const char *cursor;
    const char *payload;
    const char *action;
    char server[OPENAI_MCP_NAME_MAX];
    char tool[OPENAI_MCP_NAME_MAX];
    int written;

    if (output == NULL || output_size == 0U || arguments == NULL) {
        return 0;
    }

    cursor = arguments;
    if (!openai_mcp_call_token(&cursor, server, sizeof(server)) ||
        !openai_mcp_call_token(&cursor, tool, sizeof(tool)) ||
        !openai_mcp_name_valid(server) ||
        !openai_mcp_name_valid(tool)) {
        return 0;
    }

    payload = openai_skip_ws(cursor);
    if (!openai_mcp_call_args_valid(payload)) {
        return 0;
    }

    count = openai_mcp_parse(config, servers, NULL);
    for (index = 0U; index < count; ++index) {
        if (!openai_equal_ci(server, servers[index].name)) {
            continue;
        }

        if (openai_equal_ci(servers[index].transport, "stdio")) {
            action = "guarded subprocess transport";
        } else if (openai_equal_ci(servers[index].transport, "http")) {
            action = "guarded HTTP transport";
        } else {
            action = "guarded SSE transport";
        }

        written = snprintf(
            output, output_size,
            "OVMS Agent MCP call plan\n"
            "------------------------\n"
            "Server:     %s\n"
            "Transport:  %s\n"
            "Target:     %s\n"
            "Tool:       %s\n"
            "Arguments:  %s\n"
            "Action:     %s\n"
            "Approval:   required before transport execution\n"
            "Execution:  not performed by this command\n",
            servers[index].name,
            servers[index].transport,
            servers[index].target,
            tool,
            *payload != '\0' ? payload : "(none)",
            action
        );
        return written >= 0 && (size_t)written < output_size;
    }

    return 0;
}

int openai_mcp_text(char *output, size_t output_size)
{
    return openai_mcp_catalog_text(
        getenv("OVMS_AGENT_MCP_SERVERS"), output, output_size);
}

void openai_show_mcp(void)
{
    char output[4096];

    if (!openai_mcp_text(output, sizeof(output))) {
        (void)puts("Unable to build MCP server catalog.");
        return;
    }
    (void)fputs(output, stdout);
}

void openai_show_mcp_info(const char *arguments)
{
    char output[2048];

    if (!openai_mcp_info_text(
            getenv("OVMS_AGENT_MCP_SERVERS"),
            arguments, output, sizeof(output))) {
        (void)puts("Usage: AGENT/MCP/INFO <server-name>");
        return;
    }
    (void)fputs(output, stdout);
}

void openai_show_mcp_call(const char *arguments)
{
    char output[4096];

    if (!openai_mcp_call_text(
            getenv("OVMS_AGENT_MCP_SERVERS"),
            arguments, output, sizeof(output))) {
        (void)puts(
            "Usage: AGENT/MCP/CALL <server-name> <tool-name> [arguments]");
        return;
    }
    (void)fputs(output, stdout);
}

#define OPENAI_MCP_BRIDGE_REQUEST_FILE "OVMS_AGENT_MCP_REQUEST.TMP"
#define OPENAI_MCP_BRIDGE_RESPONSE_FILE "OVMS_AGENT_MCP_RESPONSE.TMP"
#define OPENAI_MCP_BRIDGE_COMMAND_MAX 512U

static int openai_mcp_stdio_target_valid(const char *target)
{
    const unsigned char *cursor;

    if (target == NULL || *target == '\0') {
        return 0;
    }

    cursor = (const unsigned char *)target;
    while (*cursor != '\0') {
        if (!( (*cursor >= (unsigned char)'A' && *cursor <= (unsigned char)'Z') ||
               (*cursor >= (unsigned char)'a' && *cursor <= (unsigned char)'z') ||
               (*cursor >= (unsigned char)'0' && *cursor <= (unsigned char)'9') ||
               *cursor == (unsigned char)'_' || *cursor == (unsigned char)'-' ||
               *cursor == (unsigned char)'.' || *cursor == (unsigned char)'$' ||
               *cursor == (unsigned char)':' || *cursor == (unsigned char)'[' ||
               *cursor == (unsigned char)']' || *cursor == (unsigned char)'<' ||
               *cursor == (unsigned char)'>' || *cursor == (unsigned char)'@')) {
            return 0;
        }
        ++cursor;
    }

    return 1;
}

static int openai_mcp_http_target_valid(const char *target)
{
    const unsigned char *cursor;

    if (target == NULL ||
        (strncmp(target, "http://", 7) != 0 &&
         strncmp(target, "https://", 8) != 0)) {
        return 0;
    }

    cursor = (const unsigned char *)target;
    while (*cursor != '\0') {
        if (*cursor <= (unsigned char)' ' || *cursor == (unsigned char)'"' ||
            *cursor == (unsigned char)'<' || *cursor == (unsigned char)'>' ||
            *cursor == (unsigned char)'\\' || *cursor == (unsigned char)'`') {
            return 0;
        }
        ++cursor;
    }
    return 1;
}

static int openai_mcp_file_bridge_execute(const char *bridge,
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
    int written;
    int status;

    if (!openai_mcp_stdio_target_valid(bridge) || transport == NULL ||
        target == NULL || tool == NULL || arguments == NULL ||
        result == NULL || result_size == 0U) {
        return 0;
    }

    file = fopen(OPENAI_MCP_BRIDGE_REQUEST_FILE, "w");
    if (file == NULL) {
        return 0;
    }
    if (((strcmp(transport, "stdio") == 0 &&
          fprintf(file, "tool=%s\narguments=%s\n", tool, arguments) < 0) ||
         (strcmp(transport, "stdio") != 0 &&
          fprintf(file, "transport=%s\ntarget=%s\ntool=%s\narguments=%s\n",
                  transport, target, tool, arguments) < 0)) ||
        fclose(file) != 0) {
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

    status = system(command);
    if ((status & 1) == 0) {
        (void)remove(OPENAI_MCP_BRIDGE_REQUEST_FILE);
        (void)remove(OPENAI_MCP_BRIDGE_RESPONSE_FILE);
        return 0;
    }

    file = fopen(OPENAI_MCP_BRIDGE_RESPONSE_FILE, "r");
    if (file == NULL) {
        (void)remove(OPENAI_MCP_BRIDGE_REQUEST_FILE);
        return 0;
    }

    used = 0U;
    result[0] = '\0';
    while (fgets(line, sizeof(line), file) != NULL) {
        size_t length = strlen(line);
        if (length >= result_size - used) {
            (void)fclose(file);
            (void)remove(OPENAI_MCP_BRIDGE_REQUEST_FILE);
            (void)remove(OPENAI_MCP_BRIDGE_RESPONSE_FILE);
            return 0;
        }
        memcpy(result + used, line, length);
        used += length;
        result[used] = '\0';
    }
    (void)fclose(file);
    (void)remove(OPENAI_MCP_BRIDGE_REQUEST_FILE);
    (void)remove(OPENAI_MCP_BRIDGE_RESPONSE_FILE);
    return 1;
}

static int openai_mcp_bridge_executor(const char *target,
                                      const char *tool,
                                      const char *arguments,
                                      char *result,
                                      size_t result_size,
                                      void *context)
{
    (void)context;
    if (!openai_mcp_stdio_target_valid(target)) {
        return 0;
    }
    return openai_mcp_file_bridge_execute(target, "stdio", target, tool,
                                           arguments, result, result_size);
}

static int openai_mcp_http_bridge_executor(const char *target,
                                           const char *tool,
                                           const char *arguments,
                                           char *result,
                                           size_t result_size,
                                           void *context)
{
    const char *bridge;
    (void)context;

    if (!openai_mcp_http_target_valid(target)) {
        return 0;
    }
    bridge = getenv("OVMS_AGENT_MCP_HTTP_BRIDGE");
    if (!openai_mcp_stdio_target_valid(bridge)) {
        return 0;
    }
    return openai_mcp_file_bridge_execute(bridge, "http", target, tool,
                                           arguments, result, result_size);
}


static int openai_mcp_sse_bridge_executor(const char *target,
                                          const char *tool,
                                          const char *arguments,
                                          char *result,
                                          size_t result_size,
                                          void *context)
{
    const char *bridge;
    (void)context;

    if (!openai_mcp_http_target_valid(target)) {
        return 0;
    }
    bridge = getenv("OVMS_AGENT_MCP_SSE_BRIDGE");
    if (!openai_mcp_stdio_target_valid(bridge)) {
        return 0;
    }
    return openai_mcp_file_bridge_execute(bridge, "sse", target, tool,
                                           arguments, result, result_size);
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
    openai_mcp_server_desc servers[OPENAI_MCP_MAX_SERVERS];
    unsigned int count;
    unsigned int index;
    const char *cursor;
    const char *payload;
    char server[OPENAI_MCP_NAME_MAX];
    char tool[OPENAI_MCP_NAME_MAX];
    char result[2048];
    openai_mcp_executor_fn executor;
    int written;

    if (output == NULL || output_size == 0U || arguments == NULL) {
        return 0;
    }

    if (openai_policy_source() != OPENAI_APPROVAL_FULL) {
        written = snprintf(output, output_size,
            "MCP transport execution refused: FULL approval policy is required.\n");
        return written >= 0 && (size_t)written < output_size;
    }

    cursor = arguments;
    if (!openai_mcp_call_token(&cursor, server, sizeof(server)) ||
        !openai_mcp_call_token(&cursor, tool, sizeof(tool)) ||
        !openai_mcp_name_valid(server) || !openai_mcp_name_valid(tool)) {
        return 0;
    }

    payload = openai_skip_ws(cursor);
    if (!openai_mcp_call_args_valid(payload)) {
        return 0;
    }

    count = openai_mcp_parse(config, servers, NULL);
    for (index = 0U; index < count; ++index) {
        if (!openai_equal_ci(server, servers[index].name)) {
            continue;
        }

        executor = NULL;
        if (openai_equal_ci(servers[index].transport, "stdio")) {
            if (!openai_mcp_stdio_target_valid(servers[index].target)) {
                written = snprintf(output, output_size,
                    "MCP transport execution refused: unsafe stdio bridge target.\n");
                return written >= 0 && (size_t)written < output_size;
            }
            executor = stdio_executor;
        } else if (openai_equal_ci(servers[index].transport, "http")) {
            if (!openai_mcp_http_target_valid(servers[index].target)) {
                written = snprintf(output, output_size,
                    "MCP transport execution refused: unsafe HTTP endpoint.\n");
                return written >= 0 && (size_t)written < output_size;
            }
            executor = http_executor;
        } else if (openai_equal_ci(servers[index].transport, "sse")) {
            if (!openai_mcp_http_target_valid(servers[index].target)) {
                written = snprintf(output, output_size,
                    "MCP transport execution refused: unsafe SSE endpoint.\n");
                return written >= 0 && (size_t)written < output_size;
            }
            executor = sse_executor;
        } else {
            written = snprintf(output, output_size,
                "MCP transport execution refused: unsupported transport %s.\n",
                servers[index].transport);
            return written >= 0 && (size_t)written < output_size;
        }

        if (executor == NULL) {
            written = snprintf(output, output_size,
                "MCP transport execution refused: %s executor is not configured.\n",
                servers[index].transport);
            return written >= 0 && (size_t)written < output_size;
        }

        result[0] = '\0';
        if (!executor(servers[index].target, tool, payload,
                      result, sizeof(result), context)) {
            written = snprintf(output, output_size,
                "MCP %s execution failed.\nServer: %s\nTool: %s\n",
                servers[index].transport, servers[index].name, tool);
            return written >= 0 && (size_t)written < output_size;
        }

        written = snprintf(output, output_size,
            "OVMS Agent MCP execution result\n"
            "-------------------------------\n"
            "Server:     %s\n"
            "Transport:  %s\n"
            "Target:     %s\n"
            "Tool:       %s\n"
            "Status:     success\n"
            "Result:\n%s%s",
            servers[index].name, servers[index].transport,
            servers[index].target, tool,
            *result != '\0' ? result : "(empty)\n",
            (*result != '\0' && result[strlen(result) - 1U] != '\n') ? "\n" : "");
        return written >= 0 && (size_t)written < output_size;
    }

    return 0;
}

int openai_mcp_exec_transport_text(const char *config,
                                       const char *arguments,
                                       openai_mcp_executor_fn stdio_executor,
                                       openai_mcp_executor_fn http_executor,
                                       void *context,
                                       char *output,
                                       size_t output_size)
{
    openai_mcp_server_desc servers[OPENAI_MCP_MAX_SERVERS];
    unsigned int count;
    unsigned int index;
    const char *cursor;
    const char *payload;
    char server[OPENAI_MCP_NAME_MAX];
    char tool[OPENAI_MCP_NAME_MAX];
    char result[2048];
    openai_mcp_executor_fn executor;
    int written;

    if (output == NULL || output_size == 0U || arguments == NULL) {
        return 0;
    }

    if (openai_policy_source() != OPENAI_APPROVAL_FULL) {
        written = snprintf(output, output_size,
            "MCP transport execution refused: FULL approval policy is required.\n");
        return written >= 0 && (size_t)written < output_size;
    }

    cursor = arguments;
    if (!openai_mcp_call_token(&cursor, server, sizeof(server)) ||
        !openai_mcp_call_token(&cursor, tool, sizeof(tool)) ||
        !openai_mcp_name_valid(server) || !openai_mcp_name_valid(tool)) {
        return 0;
    }

    payload = openai_skip_ws(cursor);
    if (!openai_mcp_call_args_valid(payload)) {
        return 0;
    }

    count = openai_mcp_parse(config, servers, NULL);
    for (index = 0U; index < count; ++index) {
        if (!openai_equal_ci(server, servers[index].name)) {
            continue;
        }

        executor = NULL;
        if (openai_equal_ci(servers[index].transport, "stdio")) {
            if (!openai_mcp_stdio_target_valid(servers[index].target)) {
                written = snprintf(output, output_size,
                    "MCP transport execution refused: unsafe stdio bridge target.\n");
                return written >= 0 && (size_t)written < output_size;
            }
            executor = stdio_executor;
        } else if (openai_equal_ci(servers[index].transport, "http")) {
            if (!openai_mcp_http_target_valid(servers[index].target)) {
                written = snprintf(output, output_size,
                    "MCP transport execution refused: unsafe HTTP endpoint.\n");
                return written >= 0 && (size_t)written < output_size;
            }
            executor = http_executor;
        } else {
            written = snprintf(output, output_size,
                "MCP transport execution refused: transport %s is not enabled in M244.\n",
                servers[index].transport);
            return written >= 0 && (size_t)written < output_size;
        }

        if (executor == NULL) {
            written = snprintf(output, output_size,
                "MCP transport execution refused: %s executor is not configured.\n",
                servers[index].transport);
            return written >= 0 && (size_t)written < output_size;
        }

        result[0] = '\0';
        if (!executor(servers[index].target, tool, payload,
                      result, sizeof(result), context)) {
            written = snprintf(output, output_size,
                "MCP %s execution failed.\nServer: %s\nTool: %s\n",
                servers[index].transport, servers[index].name, tool);
            return written >= 0 && (size_t)written < output_size;
        }

        written = snprintf(output, output_size,
            "OVMS Agent MCP execution result\n"
            "-------------------------------\n"
            "Server:     %s\n"
            "Transport:  %s\n"
            "Target:     %s\n"
            "Tool:       %s\n"
            "Status:     success\n"
            "Result:\n%s%s",
            servers[index].name, servers[index].transport,
            servers[index].target, tool,
            *result != '\0' ? result : "(empty)\n",
            (*result != '\0' && result[strlen(result) - 1U] != '\n') ? "\n" : "");
        return written >= 0 && (size_t)written < output_size;
    }

    return 0;
}

int openai_mcp_execute_text(const char *config,
                            const char *arguments,
                            openai_mcp_executor_fn executor,
                            void *context,
                            char *output,
                            size_t output_size)
{
    openai_mcp_server_desc servers[OPENAI_MCP_MAX_SERVERS];
    unsigned int count;
    unsigned int index;
    const char *cursor;
    const char *payload;
    char server[OPENAI_MCP_NAME_MAX];
    char tool[OPENAI_MCP_NAME_MAX];
    char result[2048];
    int written;

    if (output == NULL || output_size == 0U || arguments == NULL ||
        executor == NULL) {
        return 0;
    }

    if (openai_policy_source() != OPENAI_APPROVAL_FULL) {
        written = snprintf(output, output_size,
            "MCP transport execution refused: FULL approval policy is required.\n");
        return written >= 0 && (size_t)written < output_size;
    }

    cursor = arguments;
    if (!openai_mcp_call_token(&cursor, server, sizeof(server)) ||
        !openai_mcp_call_token(&cursor, tool, sizeof(tool)) ||
        !openai_mcp_name_valid(server) || !openai_mcp_name_valid(tool)) {
        return 0;
    }

    payload = openai_skip_ws(cursor);
    if (!openai_mcp_call_args_valid(payload)) {
        return 0;
    }

    /* M243 compatibility/security: reject an ambiguous malformed tail
       immediately after the selected catalog entry. This catches config
       injection such as "name|stdio|@BRIDGE;DELETE" instead of silently
       treating DELETE as an unrelated ignored entry. */
    if (config != NULL) {
        const char *scan = config;
        size_t server_len = strlen(server);
        while (*scan != '\0') {
            const char *entry_end = strchr(scan, ';');
            const char *first_bar = strchr(scan, '|');
            if (first_bar != NULL &&
                (entry_end == NULL || first_bar < entry_end) &&
                (size_t)(first_bar - scan) == server_len &&
                strncmp(scan, server, server_len) == 0) {
                if (entry_end != NULL && entry_end[1] != '\0') {
                    const char *next_end = strchr(entry_end + 1, ';');
                    const char *bar1 = strchr(entry_end + 1, '|');
                    const char *bar2 = NULL;
                    if (bar1 != NULL &&
                        (next_end == NULL || bar1 < next_end)) {
                        bar2 = strchr(bar1 + 1, '|');
                    }
                    if (bar1 == NULL ||
                        (next_end != NULL && bar1 > next_end) ||
                        bar2 == NULL ||
                        (next_end != NULL && bar2 > next_end)) {
                        written = snprintf(output, output_size,
                            "MCP transport execution refused: unsafe stdio bridge target.\n");
                        return written >= 0 && (size_t)written < output_size;
                    }
                }
                break;
            }
            if (entry_end == NULL) {
                break;
            }
            scan = entry_end + 1;
        }
    }

    count = openai_mcp_parse(config, servers, NULL);
    for (index = 0U; index < count; ++index) {
        if (!openai_equal_ci(server, servers[index].name)) {
            continue;
        }

        if (!openai_equal_ci(servers[index].transport, "stdio")) {
            written = snprintf(output, output_size,
                "MCP transport execution refused: transport %s is not enabled in M243.\n",
                servers[index].transport);
            return written >= 0 && (size_t)written < output_size;
        }

        if (!openai_mcp_stdio_target_valid(servers[index].target)) {
            written = snprintf(output, output_size,
                "MCP transport execution refused: unsafe stdio bridge target.\n");
            return written >= 0 && (size_t)written < output_size;
        }

        result[0] = '\0';
        if (!executor(servers[index].target, tool, payload,
                      result, sizeof(result), context)) {
            written = snprintf(output, output_size,
                "MCP stdio bridge execution failed.\n"
                "Server: %s\nTool: %s\n",
                servers[index].name, tool);
            return written >= 0 && (size_t)written < output_size;
        }

        written = snprintf(output, output_size,
            "OVMS Agent MCP execution result\n"
            "-------------------------------\n"
            "Server:     %s\n"
            "Transport:  stdio\n"
            "Target:     %s\n"
            "Tool:       %s\n"
            "Status:     success\n"
            "Result:\n%s%s",
            servers[index].name, servers[index].target, tool,
            *result != '\0' ? result : "(empty)\n",
            (*result != '\0' && result[strlen(result) - 1U] != '\n') ? "\n" : "");
        return written >= 0 && (size_t)written < output_size;
    }

    return 0;
}

static const char *openai_mcp_res_name(openai_mcp_res_status status)
{
    if (status == OPENAI_MCP_RES_SUCCESS) return "success";
    if (status == OPENAI_MCP_RES_FAILED) return "failed";
    if (status == OPENAI_MCP_RES_REFUSED) return "refused";
    return "invalid";
}

int openai_mcp_feedback_text(const openai_mcp_result *result,
                            char *output, size_t output_size)
{
    int written;

    if (result == NULL || output == NULL || output_size == 0U ||
        result->status == OPENAI_MCP_RES_INVALID) return 0;

    written = snprintf(
        output, output_size,
        "TOOL RESULT MCP\n"
        "server=%s\n"
        "transport=%s\n"
        "tool=%s\n"
        "status=%s\n"
        "detail=%s%s",
        *result->server != '\0' ? result->server : "(unknown)",
        *result->transport != '\0' ? result->transport : "(unknown)",
        *result->tool != '\0' ? result->tool : "(unknown)",
        openai_mcp_res_name(result->status),
        *result->detail != '\0' ? result->detail : "(none)\n",
        (*result->detail != '\0' &&
         result->detail[strlen(result->detail) - 1U] != '\n') ? "\n" : "");

    return written >= 0 && (size_t)written < output_size;
}

int openai_mcp_record_result(const char *arguments,
                             const openai_mcp_result *result,
                             char *output, size_t output_size)
{
    const char *status;

    if (!openai_mcp_feedback_text(result, output, output_size)) return 0;

    status = openai_mcp_res_name(result->status);
    openai_tx_model_call("mcp_call", arguments != NULL ? arguments : "");
    openai_tx_model_result("mcp_call", status, output);
    return 1;
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

int openai_parity_text(char *output, size_t output_size)
{
    int written;

    if (output == NULL || output_size == 0U) {
        return 0;
    }

    written = snprintf(
        output, output_size,
        "OVMS Agent Codex-parity status\n"
        "------------------------------\n"
        "Agent loop:           available\n"
        "Read tools:           available\n"
        "Guarded write tools:  available\n"
        "Controlled build:     available\n"
        "Plan/apply workflow:  available\n"
        "Repair retry:         available\n"
        "Tool discovery:       available\n"
        "Approval profiles:    available\n"
        "Unified EXEC entry:   available\n"
        "Dry-run planning:     available\n"
        "Persistent sessions:  available\n"
        "Session resume/fork:  available\n"
        "Archive lifecycle:    available\n"
        "Session transcripts:   available\n"
        "Direct tool runner:    available\n"
        "Resume/fork execution: available\n"
        "Autonomous tool loop:  available\n"
        "Tool-result feedback:  available\n"
        "Bounded multi-patch:   available\n"
        "Context restoration:  available\n"
        "Fork ancestry context: available\n"
        "Project instructions:  available\n"
        "Instruction reload:    available\n"
        "Repository map:       available\n"
        "Context preloading:   available\n"
        "Git state context:     available\n"
        "Git diff awareness:    available\n"
        "Multi-hunk patching:   available\n"
        "Patch prevalidation:   available\n"
        "Autonomous multi-hunk:  available\n"
        "Normalized results:     available\n"
        "Result persistence:     available\n"
        "Normalized build result: available\n"
        "Build evidence replay:  available\n"
        "Session result history: available\n"
        "Evidence preloading:    available\n"
        "Priority result replay: available\n"
        "MCP/tool servers:     not yet implemented\n"
        "Unix sandbox parity:  not applicable on OpenVMS\n"
    );

    return written >= 0 && (size_t)written < output_size;
}

void openai_show_parity(void)
{
    char output[4096];

    if (!openai_parity_text(output, sizeof(output))) {
        (void)puts("Unable to build parity status.");
        return;
    }

    (void)fputs(output, stdout);
}

int openai_final_parity_text(char *output, size_t output_size)
{
    int written;

    if (output == NULL || output_size == 0U) {
        return 0;
    }

    written = snprintf(
        output, output_size,
        "OVMS Agent final Codex parity report\n"
        "===================================\n"
        "Scope: practical local coding-agent behavior on OpenVMS\n"
        "Result: practical parity achieved with documented adaptations\n"
        "\n"
        "Core coding loop\n"
        "----------------\n"
        "Repository read/search:        native\n"
        "Guarded create/edit/patch:     native\n"
        "Controlled build/test:         native\n"
        "Plan/apply/repair retry:       native\n"
        "Code review workflow:          native\n"
        "Autonomous tool loop:          native\n"
        "Normalized tool feedback:      native\n"
        "\n"
        "Session and context\n"
        "-------------------\n"
        "Persistent sessions:           native\n"
        "Resume/rename/archive/delete:  native\n"
        "Fork and delegated branch:     adapted (session fork + EXEC/FORK)\n"
        "Transcript/result history:     native\n"
        "Context restoration/replay:    native\n"
        "Project instructions/reload:   adapted (OpenVMS project policy)\n"
        "Repository/Git context:        native\n"
        "\n"
        "Permissions and execution\n"
        "-------------------------\n"
        "Read/workspace/full policy:    adapted\n"
        "Session policy override:       native\n"
        "Environment policy default:   native\n"
        "Workspace path guards:         native\n"
        "Unix/Windows OS sandbox:       not applicable on OpenVMS\n"
        "Danger-full-access analogue:   full policy + explicit write enable\n"
        "\n"
        "External tools and current context\n"
        "----------------------------------\n"
        "MCP server catalog:            native\n"
        "MCP stdio transport:           adapted through configured bridge\n"
        "MCP HTTP/SSE transport:        adapted through configured bridge\n"
        "MCP approval enforcement:      native\n"
        "MCP result/session evidence:   native\n"
        "MCP protocol/auth lifecycle:   external bridge responsibility\n"
        "Web/current-data tools:        external through MCP\n"
        "Plugins/connectors:            external through MCP\n"
        "Reusable skills/prompts:       adapted through project instructions\n"
        "GitHub Git transport:          adapted through OpenVMS Git\n"
        "GitHub issues/pull requests:   external through configured bridge\n"
        "GitHub credential handling:    external to OVMS Agent core\n"
        "\n"
        "Platform-specific exclusions\n"
        "----------------------------\n"
        "TUI keymaps/Vim/composer UI:   not applicable\n"
        "IDE/desktop-app integration:   not applicable\n"
        "Unix shell completion:         not applicable\n"
        "Windows sandbox setup:         not applicable\n"
        "Codex cloud handoff:           external service, not local core\n"
        "Image attachment UI:           external client concern\n"
        "\n"
        "OpenVMS adaptation boundary\n"
        "---------------------------\n"
        "DCL replaces generated shell execution.\n"
        "RMS versions provide guarded rollback semantics.\n"
        "Configured bridges isolate external transports and credentials.\n"
        "Session forks provide bounded delegated-work branches without\n"
        "requiring Unix worktrees or process sandbox primitives.\n"
        "\n"
        "Compatibility: legacy AGENT/PARITY remains frozen for M227 tests.\n"
    );

    return written >= 0 && (size_t)written < output_size;
}

void openai_show_final_parity(void)
{
    char output[8192];

    if (!openai_final_parity_text(output, sizeof(output))) {
        (void)puts("Unable to build final parity report.");
        return;
    }

    (void)fputs(output, stdout);
}

void openai_exec_context(agent_state *state,
                         const char *goal)
{
    char model_goal[8192];
    int policy;

    if (state == NULL ||
        goal == NULL ||
        *openai_skip_ws(goal) == '\0') {
        (void)puts("A continuation goal is required.");
        return;
    }

    if (!openai_context_build(
            goal, model_goal, sizeof(model_goal))) {
        (void)puts("Unable to rebuild persistent session context.");
        return;
    }

    policy = openai_policy_source();

    if (policy == OPENAI_APPROVAL_READ) {
        if (!openai_session_note_goal(goal)) {
            (void)puts("Unable to persist current session goal.");
            return;
        }

        openai_tx_note_exec("CONTEXT", goal, "started");
        openai_agent(state, model_goal);
        return;
    }

    if (!state->write_enabled) {
        (void)puts(
            "Context continuation with workspace/full policy "
            "requires OVMS_AGENT_WRITE_ENABLED."
        );
        return;
    }

    if (!openai_session_note_goal(goal)) {
        (void)puts("Unable to persist current session goal.");
        return;
    }

    openai_tx_note_exec("CONTEXT", goal, "started");

    openai_agent_write(state, model_goal);
}

void openai_exec_goal(agent_state *state, const char *goal)
{
    int policy;

    if (state == NULL ||
        goal == NULL ||
        *openai_skip_ws(goal) == '\0') {
        (void)puts("Usage: AGENT/EXEC <goal>");
        return;
    }

    policy = openai_policy_source();

    if (policy == OPENAI_APPROVAL_READ) {
        if (!openai_session_note_goal(goal)) {
            (void)puts("Unable to persist current session goal.");
            return;
        }
        openai_tx_note_exec("EXEC", goal, "started");
        openai_agent(state, goal);
        return;
    }

    if (!state->write_enabled) {
        (void)puts(
            "AGENT/EXEC workspace/full policy requires "
            "OVMS_AGENT_WRITE_ENABLED."
        );
        return;
    }

    if (!openai_session_note_goal(goal)) {
        (void)puts("Unable to persist current session goal.");
        return;
    }

    openai_tx_note_exec("EXEC", goal, "started");

    openai_agent_write(state, goal);
}

void openai_exec_dry(agent_state *state, const char *goal)
{
    if (state == NULL ||
        goal == NULL ||
        *openai_skip_ws(goal) == '\0') {
        (void)puts("Usage: AGENT/EXEC/DRY <goal>");
        return;
    }

    if (!openai_session_note_goal(goal)) {
        (void)puts("Unable to persist current session goal.");
        return;
    }

    openai_tx_note_exec("DRY", goal, "started");
    openai_agent_plan(state, goal);
}

void openai_test_reset_approval(void)
{
    openai_approval_override = -1;
}

static void openai_mcp_res_clear(openai_mcp_result *result)
{
    if (result == NULL) return;
    memset(result, 0, sizeof(*result));
    result->status = OPENAI_MCP_RES_INVALID;
}

static void openai_mcp_res_copy(char *dest, size_t dest_size,
                                const char *source)
{
    if (dest == NULL || dest_size == 0U) return;
    if (source == NULL) source = "";
    (void)snprintf(dest, dest_size, "%s", source);
}

int openai_mcp_run_result(const char *config,
                          const char *arguments,
                          openai_mcp_executor_fn stdio_executor,
                          openai_mcp_executor_fn http_executor,
                          openai_mcp_executor_fn sse_executor,
                          void *context,
                          openai_mcp_result *result)
{
    openai_mcp_server_desc servers[OPENAI_MCP_MAX_SERVERS];
    unsigned int count;
    unsigned int index;
    const char *cursor;
    const char *payload;
    char server[OPENAI_MCP_NAME_MAX];
    char tool[OPENAI_MCP_NAME_MAX];
    char bridge_result[OPENAI_MCP_RES_TEXT_MAX];
    openai_mcp_executor_fn executor;

    if (result == NULL) return 0;
    openai_mcp_res_clear(result);
    if (config == NULL || arguments == NULL) return 0;

    cursor = arguments;
    if (!openai_mcp_call_token(&cursor, server, sizeof(server)) ||
        !openai_mcp_call_token(&cursor, tool, sizeof(tool)) ||
        !openai_mcp_name_valid(server) || !openai_mcp_name_valid(tool)) {
        return 0;
    }
    payload = openai_skip_ws(cursor);
    if (!openai_mcp_call_args_valid(payload)) return 0;

    openai_mcp_res_copy(result->server, sizeof(result->server), server);
    openai_mcp_res_copy(result->tool, sizeof(result->tool), tool);

    if (openai_policy_source() != OPENAI_APPROVAL_FULL) {
        result->status = OPENAI_MCP_RES_REFUSED;
        openai_mcp_res_copy(result->detail, sizeof(result->detail),
                            "FULL approval policy is required.");
        return 1;
    }

    count = openai_mcp_parse(config, servers, NULL);
    for (index = 0U; index < count; ++index) {
        if (!openai_equal_ci(server, servers[index].name)) continue;

        openai_mcp_res_copy(result->server, sizeof(result->server),
                            servers[index].name);
        openai_mcp_res_copy(result->transport, sizeof(result->transport),
                            servers[index].transport);
        executor = NULL;

        if (openai_equal_ci(servers[index].transport, "stdio")) {
            if (!openai_mcp_stdio_target_valid(servers[index].target)) {
                result->status = OPENAI_MCP_RES_REFUSED;
                openai_mcp_res_copy(result->detail, sizeof(result->detail),
                                    "unsafe stdio bridge target.");
                return 1;
            }
            executor = stdio_executor;
        } else if (openai_equal_ci(servers[index].transport, "http")) {
            if (!openai_mcp_http_target_valid(servers[index].target)) {
                result->status = OPENAI_MCP_RES_REFUSED;
                openai_mcp_res_copy(result->detail, sizeof(result->detail),
                                    "unsafe HTTP endpoint.");
                return 1;
            }
            executor = http_executor;
        } else if (openai_equal_ci(servers[index].transport, "sse")) {
            if (!openai_mcp_http_target_valid(servers[index].target)) {
                result->status = OPENAI_MCP_RES_REFUSED;
                openai_mcp_res_copy(result->detail, sizeof(result->detail),
                                    "unsafe SSE endpoint.");
                return 1;
            }
            executor = sse_executor;
        } else {
            result->status = OPENAI_MCP_RES_REFUSED;
            openai_mcp_res_copy(result->detail, sizeof(result->detail),
                                "unsupported transport.");
            return 1;
        }

        if (executor == NULL) {
            result->status = OPENAI_MCP_RES_REFUSED;
            openai_mcp_res_copy(result->detail, sizeof(result->detail),
                                "transport executor is not configured.");
            return 1;
        }

        bridge_result[0] = '\0';
        if (!executor(servers[index].target, tool, payload,
                      bridge_result, sizeof(bridge_result), context)) {
            result->status = OPENAI_MCP_RES_FAILED;
            openai_mcp_res_copy(result->detail, sizeof(result->detail),
                                "transport execution failed.");
            return 1;
        }

        result->status = OPENAI_MCP_RES_SUCCESS;
        openai_mcp_res_copy(result->detail, sizeof(result->detail),
                            *bridge_result != '\0' ? bridge_result : "(empty)");
        return 1;
    }

    result->status = OPENAI_MCP_RES_REFUSED;
    openai_mcp_res_copy(result->detail, sizeof(result->detail),
                        "server is not configured.");
    return 1;
}

int openai_mcp_result_text(const openai_mcp_result *result,
                           char *output, size_t output_size)
{
    const char *status;
    int written;

    if (result == NULL || output == NULL || output_size == 0U ||
        result->status == OPENAI_MCP_RES_INVALID) return 0;

    if (result->status == OPENAI_MCP_RES_SUCCESS) status = "success";
    else if (result->status == OPENAI_MCP_RES_FAILED) status = "failed";
    else status = "refused";

    written = snprintf(output, output_size,
        "OVMS Agent MCP normalized result\n"
        "--------------------------------\n"
        "Server:     %s\n"
        "Transport:  %s\n"
        "Tool:       %s\n"
        "Status:     %s\n"
        "Detail:\n%s%s",
        *result->server != '\0' ? result->server : "(unknown)",
        *result->transport != '\0' ? result->transport : "(unknown)",
        *result->tool != '\0' ? result->tool : "(unknown)",
        status,
        *result->detail != '\0' ? result->detail : "(none)\n",
        (*result->detail != '\0' &&
         result->detail[strlen(result->detail) - 1U] != '\n') ? "\n" : "");
    return written >= 0 && (size_t)written < output_size;
}

#define OPENAI_LANG_OUT_MAX 4096U

typedef struct openai_lang_desc {
    const char *name;
    const char *aliases;
    const char *extensions;
    const char *compile_hint;
    const char *comment_hint;
    const char *notes;
} openai_lang_desc;

static const openai_lang_desc openai_langs[] = {
    { "C", "c,dec-c,vsi-c", ".C,.H",
      "CC source.C; LINK object",
      "/* ... */ and // where supported by the selected C dialect",
      "Preserve DEC/VSI C dialect, OpenVMS RTL usage, and the 31-character external-name constraint when targeting older DEC C/VAX toolchains." },
    { "DCL", "dcl,command-procedure,com", ".COM",
      "No compiler; execute a command procedure with @file.COM",
      "$! comment",
      "Treat DCL as an OpenVMS command language, not as a Unix shell. Preserve lexical functions, logical names, symbols, and OpenVMS file specifications." },
    { "MACRO-32", "macro,macro32,mar", ".MAR",
      "MACRO source.MAR; LINK object",
      "; comment",
      "Preserve VAX MACRO/MACRO-32 calling conventions, registers, psects, entry masks, condition values, and architecture assumptions." },
    { "FORTRAN", "fortran,fortran77,fortran90,f77,f90", ".FOR,.F,.F90",
      "FORTRAN source.FOR; LINK object",
      "! comment; older fixed-form sources may use column-based comment conventions",
      "Detect fixed-form versus free-form style before editing. Preserve continuation, column, COMMON, INCLUDE, record, and OpenVMS extension conventions already present." },
    { "BASIC", "basic,vsi-basic,vax-basic", ".BAS",
      "BASIC source.BAS; LINK object",
      "! or REM, according to the source dialect",
      "Preserve line labels, declarations, record/layout conventions, and OpenVMS BASIC extensions already used by the program." },
    { "PASCAL", "pascal,vsi-pascal,vax-pascal", ".PAS",
      "PASCAL source.PAS; LINK object",
      "{ ... } or (* ... *)",
      "Preserve module/program structure, declarations, OpenVMS attributes, and the source's existing Pascal dialect." },
    { "COBOL", "cobol,vsi-cobol,vax-cobol", ".COB,.CBL",
      "COBOL source.COB; LINK object",
      "Fixed-format indicator comments or *> where supported by the source dialect",
      "Preserve divisions, sections, paragraph names, fixed/free source format, record layouts, and OpenVMS COBOL extensions already present." },
    { "BLISS", "bliss,bliss32,bli", ".BLI,.REQ",
      "Use the installed BLISS compiler appropriate to the target architecture; LINK the resulting object",
      "! comment",
      "Preserve BLISS dialect, linkage declarations, OWN/GLOBAL/EXTERNAL storage, macros, REQUIRE files, and architecture-specific constructs. Compiler command names vary by BLISS family and platform." }
};

static int openai_lang_ci_equal(const char *left, const char *right)
{
    unsigned char a;
    unsigned char b;

    if (left == NULL || right == NULL) {
        return 0;
    }

    while (*left != '\0' && *right != '\0') {
        a = (unsigned char)*left++;
        b = (unsigned char)*right++;
        if (tolower((int)a) != tolower((int)b)) {
            return 0;
        }
    }

    return *left == '\0' && *right == '\0';
}

static int openai_lang_alias_has(const char *aliases, const char *name)
{
    const char *start;
    const char *end;
    size_t length;
    char token[64];

    if (aliases == NULL || name == NULL || *name == '\0') {
        return 0;
    }

    start = aliases;
    while (*start != '\0') {
        end = start;
        while (*end != '\0' && *end != ',') {
            ++end;
        }
        length = (size_t)(end - start);
        if (length > 0U && length < sizeof(token)) {
            (void)memcpy(token, start, length);
            token[length] = '\0';
            if (openai_lang_ci_equal(token, name)) {
                return 1;
            }
        }
        start = (*end == ',') ? end + 1 : end;
    }

    return 0;
}

static const openai_lang_desc *openai_lang_by_name(const char *name)
{
    size_t index;

    if (name == NULL || *name == '\0') {
        return NULL;
    }

    for (index = 0U; index < sizeof(openai_langs) / sizeof(openai_langs[0]); ++index) {
        if (openai_lang_ci_equal(openai_langs[index].name, name) ||
            openai_lang_alias_has(openai_langs[index].aliases, name)) {
            return &openai_langs[index];
        }
    }

    return NULL;
}

static const char *openai_lang_file_ext(const char *path)
{
    const char *cursor;
    const char *dot;
    const char *semi;

    if (path == NULL || *path == '\0') {
        return NULL;
    }

    dot = NULL;
    semi = NULL;
    cursor = path;
    while (*cursor != '\0') {
        if (*cursor == '.') {
            dot = cursor;
        } else if (*cursor == ';') {
            semi = cursor;
        }
        ++cursor;
    }

    if (dot == NULL) {
        return NULL;
    }
    if (semi != NULL && semi < dot) {
        return NULL;
    }

    return dot;
}

static int openai_lang_ext_has(const char *extensions, const char *ext)
{
    const char *start;
    const char *end;
    size_t length;
    char token[16];
    char clean[16];
    const char *semi;
    size_t clean_len;

    if (extensions == NULL || ext == NULL) {
        return 0;
    }

    semi = strchr(ext, ';');
    clean_len = semi == NULL ? strlen(ext) : (size_t)(semi - ext);
    if (clean_len == 0U || clean_len >= sizeof(clean)) {
        return 0;
    }
    (void)memcpy(clean, ext, clean_len);
    clean[clean_len] = '\0';

    start = extensions;
    while (*start != '\0') {
        end = start;
        while (*end != '\0' && *end != ',') {
            ++end;
        }
        length = (size_t)(end - start);
        if (length > 0U && length < sizeof(token)) {
            (void)memcpy(token, start, length);
            token[length] = '\0';
            if (openai_lang_ci_equal(token, clean)) {
                return 1;
            }
        }
        start = (*end == ',') ? end + 1 : end;
    }

    return 0;
}

static const openai_lang_desc *openai_lang_by_path(const char *path)
{
    const char *ext;
    size_t index;

    ext = openai_lang_file_ext(path);
    if (ext == NULL) {
        return NULL;
    }

    for (index = 0U; index < sizeof(openai_langs) / sizeof(openai_langs[0]); ++index) {
        if (openai_lang_ext_has(openai_langs[index].extensions, ext)) {
            return &openai_langs[index];
        }
    }

    return NULL;
}

static int openai_lang_write(char *output, size_t output_size,
                             size_t *used, const char *text)
{
    size_t length;

    if (output == NULL || output_size == 0U || used == NULL || text == NULL ||
        *used >= output_size) {
        return 0;
    }

    length = strlen(text);
    if (length >= output_size - *used) {
        length = output_size - *used - 1U;
    }
    if (length > 0U) {
        (void)memcpy(output + *used, text, length);
        *used += length;
    }
    output[*used] = '\0';
    return 1;
}

int openai_lang_list_text(char *output, size_t output_size)
{
    size_t index;
    size_t used;
    char line[256];
    int written;

    if (output == NULL || output_size == 0U) {
        return 0;
    }

    output[0] = '\0';
    used = 0U;
    (void)openai_lang_write(output, output_size, &used,
        "OVMS Agent language support\n"
        "---------------------------\n"
        "Language     Typical OpenVMS source types\n");

    for (index = 0U; index < sizeof(openai_langs) / sizeof(openai_langs[0]); ++index) {
        written = snprintf(line, sizeof(line), "%-12s %s\n",
                           openai_langs[index].name,
                           openai_langs[index].extensions);
        if (written < 0) {
            return 0;
        }
        (void)openai_lang_write(output, output_size, &used, line);
    }

    (void)openai_lang_write(output, output_size, &used,
        "\nDetection is case-insensitive and accepts OpenVMS file-version suffixes such as ;1.\n"
        "Compiler availability is not assumed; project build procedures remain authoritative.\n");
    return 1;
}

int openai_lang_info_text(const char *arguments,
                          char *output, size_t output_size)
{
    const openai_lang_desc *language;
    size_t used;
    char line[768];
    int written;

    if (arguments == NULL || *arguments == '\0' || output == NULL || output_size == 0U) {
        return 0;
    }

    language = openai_lang_by_name(arguments);
    if (language == NULL) {
        language = openai_lang_by_path(arguments);
    }
    if (language == NULL) {
        return 0;
    }

    output[0] = '\0';
    used = 0U;
    written = snprintf(line, sizeof(line),
        "Language: %s\nAliases: %s\nSource types: %s\nBuild hint: %s\nComments: %s\nGuidance: %s\n",
        language->name, language->aliases, language->extensions,
        language->compile_hint, language->comment_hint, language->notes);
    if (written < 0) {
        return 0;
    }
    (void)openai_lang_write(output, output_size, &used, line);
    return 1;
}

int openai_lang_detect_text(const char *path,
                            char *output, size_t output_size)
{
    const openai_lang_desc *language;
    int written;

    if (path == NULL || *path == '\0' || output == NULL || output_size == 0U) {
        return 0;
    }

    language = openai_lang_by_path(path);
    if (language == NULL) {
        written = snprintf(output, output_size,
                           "Language: unknown\nPath: %s\n",
                           path);
    } else {
        written = snprintf(output, output_size,
                           "Language: %s\nPath: %s\nBuild hint: %s\n",
                           language->name, path, language->compile_hint);
    }

    return written >= 0 && (size_t)written < output_size;
}

int openai_lang_policy_text(char *output, size_t output_size)
{
    int written;

    if (output == NULL || output_size == 0U) {
        return 0;
    }

    written = snprintf(output, output_size,
        "OVMS AGENT MULTILINGUAL POLICY\n"
        "Recognize and work natively with these historically important OpenVMS languages: "
        "C, DCL, MACRO-32, Fortran, BASIC, Pascal, COBOL, and BLISS.\n"
        "Infer language from source type and source syntax. Do not assume C syntax for non-C files. "
        "Preserve the file's existing dialect, formatting model, comments, calling conventions, record/layout rules, and OpenVMS extensions. "
        "For DCL use OpenVMS/DCL semantics, never Unix-shell assumptions. "
        "For Fortran and COBOL detect fixed versus free source form before editing. "
        "For MACRO-32 and BLISS preserve architecture and linkage assumptions. "
        "Compiler availability varies by OpenVMS system and architecture; use the project's established build procedure when present and do not invent an installed compiler. "
        "When C is involved on legacy DEC C/VAX targets, keep external linker-visible identifiers within 31 characters.\n");

    return written >= 0 && (size_t)written < output_size;
}

void openai_show_langs(void)
{
    char output[OPENAI_LANG_OUT_MAX];
    if (openai_lang_list_text(output, sizeof(output))) {
        (void)puts(output);
    }
}

void openai_show_lang_info(const char *arguments)
{
    char output[OPENAI_LANG_OUT_MAX];
    if (!openai_lang_info_text(arguments, output, sizeof(output))) {
        (void)puts("Usage: AGENT/LANG/INFO language-or-source-file");
        return;
    }
    (void)puts(output);
}

void openai_show_lang_detect(const char *path)
{
    char output[OPENAI_LANG_OUT_MAX];
    if (!openai_lang_detect_text(path, output, sizeof(output))) {
        (void)puts("Usage: AGENT/LANG/DETECT source-file");
        return;
    }
    (void)puts(output);
}

void openai_show_lang_policy(void)
{
    char output[OPENAI_LANG_OUT_MAX];
    if (openai_lang_policy_text(output, sizeof(output))) {
        (void)puts(output);
    }
}

/* M250: guarded GitHub integration. */
#define OPENAI_GH_REQ_FILE "OVMS_AGENT_GH_REQUEST.TMP"
#define OPENAI_GH_RESP_FILE "OVMS_AGENT_GH_RESPONSE.TMP"
#define OPENAI_GH_CMD_MAX 1024U
#define OPENAI_GH_ARG_MAX 1024U
#define OPENAI_GH_ENV_COM "OVMS_AGENT_GH_ENV.COM"
#define OPENAI_GH_ENV_TMP "OVMS_AGENT_GH_ENV.TMP"

static int openai_gh_no_newline(const char *text)
{
    const unsigned char *cursor;

    if (text == NULL) {
        return 0;
    }
    cursor = (const unsigned char *)text;
    while (*cursor != '\0') {
        if (*cursor == (unsigned char)'\r' || *cursor == (unsigned char)'\n') {
            return 0;
        }
        ++cursor;
    }
    return 1;
}

static int openai_gh_token_valid(const char *text)
{
    const unsigned char *cursor;

    if (text == NULL || *text == '\0' || text[0] == '-' ||
        strstr(text, "..") != NULL || strstr(text, "@{") != NULL) {
        return 0;
    }
    cursor = (const unsigned char *)text;
    while (*cursor != '\0') {
        if (!( (*cursor >= (unsigned char)'A' && *cursor <= (unsigned char)'Z') ||
               (*cursor >= (unsigned char)'a' && *cursor <= (unsigned char)'z') ||
               (*cursor >= (unsigned char)'0' && *cursor <= (unsigned char)'9') ||
               *cursor == (unsigned char)'_' || *cursor == (unsigned char)'-' ||
               *cursor == (unsigned char)'.' || *cursor == (unsigned char)'/')) {
            return 0;
        }
        ++cursor;
    }
    return 1;
}

static int openai_gh_path_valid(const char *text)
{
    const unsigned char *cursor;

    if (text == NULL || *text == '\0' || text[0] == '-' ||
        strstr(text, "..") != NULL) {
        return 0;
    }
    cursor = (const unsigned char *)text;
    while (*cursor != '\0') {
        if (!( (*cursor >= (unsigned char)'A' && *cursor <= (unsigned char)'Z') ||
               (*cursor >= (unsigned char)'a' && *cursor <= (unsigned char)'z') ||
               (*cursor >= (unsigned char)'0' && *cursor <= (unsigned char)'9') ||
               *cursor == (unsigned char)'_' || *cursor == (unsigned char)'-' ||
               *cursor == (unsigned char)'.' || *cursor == (unsigned char)'$' ||
               *cursor == (unsigned char)':' || *cursor == (unsigned char)'[' ||
               *cursor == (unsigned char)']' || *cursor == (unsigned char)'<' ||
               *cursor == (unsigned char)'>' || *cursor == (unsigned char)'/')) {
            return 0;
        }
        ++cursor;
    }
    return 1;
}

static int openai_gh_url_valid(const char *text)
{
    const unsigned char *cursor;
    const char *body;

    if (text == NULL) {
        return 0;
    }
    if (strncmp(text, "https://github.com/", 19) == 0) {
        body = text + 19;
    } else if (strncmp(text, "git@github.com:", 15) == 0) {
        body = text + 15;
    } else {
        return 0;
    }
    if (*body == '\0' || strchr(body, '/') == NULL || strstr(body, "..") != NULL) {
        return 0;
    }
    cursor = (const unsigned char *)body;
    while (*cursor != '\0') {
        if (!( (*cursor >= (unsigned char)'A' && *cursor <= (unsigned char)'Z') ||
               (*cursor >= (unsigned char)'a' && *cursor <= (unsigned char)'z') ||
               (*cursor >= (unsigned char)'0' && *cursor <= (unsigned char)'9') ||
               *cursor == (unsigned char)'_' || *cursor == (unsigned char)'-' ||
               *cursor == (unsigned char)'.' || *cursor == (unsigned char)'/')) {
            return 0;
        }
        ++cursor;
    }
    return 1;
}

static int openai_gh_split2(const char *arguments,
                            char *first, size_t first_size,
                            char *second, size_t second_size,
                            int second_optional)
{
    const char *cursor;
    const char *start;
    size_t length;

    if (first == NULL || first_size == 0U ||
        second == NULL || second_size == 0U) {
        return 0;
    }
    first[0] = '\0';
    second[0] = '\0';
    cursor = openai_skip_ws(arguments);
    if (*cursor == '\0') {
        return 0;
    }
    start = cursor;
    while (*cursor != '\0' && *cursor != ' ' && *cursor != '\t') {
        ++cursor;
    }
    length = (size_t)(cursor - start);
    if (length == 0U || length >= first_size) {
        return 0;
    }
    memcpy(first, start, length);
    first[length] = '\0';
    cursor = openai_skip_ws(cursor);
    if (*cursor == '\0') {
        return second_optional;
    }
    start = cursor;
    while (*cursor != '\0' && *cursor != ' ' && *cursor != '\t') {
        ++cursor;
    }
    length = (size_t)(cursor - start);
    if (length == 0U || length >= second_size) {
        return 0;
    }
    memcpy(second, start, length);
    second[length] = '\0';
    cursor = openai_skip_ws(cursor);
    return *cursor == '\0';
}

static int openai_gh_bridge_target(const char *target)
{
    const unsigned char *cursor;

    if (target == NULL || *target == '\0') {
        return 0;
    }
    cursor = (const unsigned char *)target;
    while (*cursor != '\0') {
        if (!( (*cursor >= (unsigned char)'A' && *cursor <= (unsigned char)'Z') ||
               (*cursor >= (unsigned char)'a' && *cursor <= (unsigned char)'z') ||
               (*cursor >= (unsigned char)'0' && *cursor <= (unsigned char)'9') ||
               *cursor == (unsigned char)'_' || *cursor == (unsigned char)'-' ||
               *cursor == (unsigned char)'.' || *cursor == (unsigned char)'$' ||
               *cursor == (unsigned char)':' || *cursor == (unsigned char)'[' ||
               *cursor == (unsigned char)']' || *cursor == (unsigned char)'<' ||
               *cursor == (unsigned char)'>' || *cursor == (unsigned char)'@')) {
            return 0;
        }
        ++cursor;
    }
    return 1;
}


static int openai_gh_has_word(const char *text, const char *word)
{
    const char *cursor;
    size_t word_len;

    if (text == NULL || word == NULL || *word == '\0') {
        return 0;
    }
    word_len = strlen(word);
    cursor = text;
    while (*cursor != '\0') {
        while (*cursor == ' ' || *cursor == '\t' || *cursor == ',') {
            ++cursor;
        }
        if (strncmp(cursor, word, word_len) == 0 &&
            (cursor[word_len] == '\0' || cursor[word_len] == ' ' ||
             cursor[word_len] == '\t' || cursor[word_len] == ',' ||
             cursor[word_len] == '\r' || cursor[word_len] == '\n')) {
            return 1;
        }
        while (*cursor != '\0' && *cursor != ',' &&
               *cursor != ' ' && *cursor != '\t' &&
               *cursor != '\r' && *cursor != '\n') {
            ++cursor;
        }
    }
    return 0;
}

static int openai_gh_env_probe(char *result, size_t result_size)
{
    FILE *file;
    char line[512];
    char priv[1024];
    char parse[64];
    char arch[64];
    int written;
    int status;
    int have_share;
    int have_parse;
    int parse_needed;

    if (result == NULL || result_size == 0U) {
        return 0;
    }
    result[0] = '\0';
    priv[0] = '\0';
    parse[0] = '\0';
    arch[0] = '\0';

    file = fopen(OPENAI_GH_ENV_COM, "w");
    if (file == NULL) {
        return 0;
    }
    if (fputs("$ ON ERROR THEN GOTO DONE\n", file) == EOF ||
        fputs("$ OPEN/WRITE GHOUT OVMS_AGENT_GH_ENV.TMP\n", file) == EOF ||
        fputs("$ P = F$GETJPI(\"\",\"CURPRIV\")\n", file) == EOF ||
        fputs("$ A = F$GETSYI(\"ARCH_NAME\")\n", file) == EOF ||
        fputs("$ WRITE GHOUT \"CURPRIV=''P'\"\n", file) == EOF ||
        fputs("$ WRITE GHOUT \"ARCH=''A'\"\n", file) == EOF ||
        fputs("$ IF A .EQS. \"VAX\" THEN GOTO DONE\n", file) == EOF ||
        fputs("$ S = F$GETJPI(\"\",\"PARSE_STYLE_PERM\")\n", file) == EOF ||
        fputs("$ WRITE GHOUT \"PARSE=''S'\"\n", file) == EOF ||
        fputs("$ DONE:\n", file) == EOF ||
        fputs("$ IF F$TRNLNM(\"GHOUT\") .NES. \"\" THEN CLOSE GHOUT\n", file) == EOF ||
        fputs("$ EXIT 1\n", file) == EOF ||
        fclose(file) != 0) {
        (void)remove(OPENAI_GH_ENV_COM);
        return 0;
    }

    (void)remove(OPENAI_GH_ENV_TMP);
    status = system("@OVMS_AGENT_GH_ENV.COM");
    (void)remove(OPENAI_GH_ENV_COM);
    if ((status & 1) == 0) {
        (void)remove(OPENAI_GH_ENV_TMP);
        return 0;
    }
    file = fopen(OPENAI_GH_ENV_TMP, "r");
    if (file == NULL) {
        return 0;
    }
    while (fgets(line, sizeof(line), file) != NULL) {
        size_t length;
        char *value;

        length = strlen(line);
        while (length > 0U &&
               (line[length - 1U] == '\r' || line[length - 1U] == '\n')) {
            line[--length] = '\0';
        }
        if (strncmp(line, "CURPRIV=", 8) == 0) {
            value = line + 8;
            (void)snprintf(priv, sizeof(priv), "%s", value);
        } else if (strncmp(line, "PARSE=", 6) == 0) {
            value = line + 6;
            (void)snprintf(parse, sizeof(parse), "%s", value);
        } else if (strncmp(line, "ARCH=", 5) == 0) {
            value = line + 5;
            (void)snprintf(arch, sizeof(arch), "%s", value);
        }
    }
    (void)fclose(file);
    (void)remove(OPENAI_GH_ENV_TMP);

    have_share = openai_gh_has_word(priv, "SHARE");
    parse_needed = strcmp(arch, "VAX") != 0;
    have_parse = !parse_needed || strcmp(parse, "EXTENDED") == 0;

    if (have_share && have_parse) {
        written = snprintf(result, result_size,
            "OpenVMS Git network preflight: ready.\n"
            "SHARE privilege: enabled\n"
            "DCL parse style: %s\n",
            parse_needed ? "EXTENDED" : "not applicable on VAX");
        return written >= 0 && (size_t)written < result_size;
    }

    written = snprintf(result, result_size,
        "GitHub network operation refused by OpenVMS preflight.\n"
        "SHARE privilege: %s\n"
        "DCL parse style: %s\n"
        "%s%s"
        "Then retry the GitHub operation.\n",
        have_share ? "enabled" : "NOT enabled",
        parse_needed ? (have_parse ? "EXTENDED" : "NOT EXTENDED") :
                       "not applicable on VAX",
        have_share ? "" :
          "At DCL run: $ SET PROCESS/PRIVILEGE=SHARE\n",
        (!parse_needed || have_parse) ? "" :
          "At DCL run: $ SET PROCESS/PARSE_STYLE=EXTENDED\n");
    return written < 0 || (size_t)written >= result_size ? 0 : -1;
}

static int openai_gh_bridge_exec(const char *operation,
                                 const char *arguments,
                                 char *result, size_t result_size)
{
    const char *bridge;
    FILE *file;
    char command[OPENAI_GH_CMD_MAX];
    char line[512];
    size_t used;
    int written;
    int status;

    bridge = getenv("OVMS_AGENT_GITHUB_BRIDGE");
    if (!openai_gh_bridge_target(bridge) || !openai_gh_no_newline(arguments)) {
        return 0;
    }
    file = fopen(OPENAI_GH_REQ_FILE, "w");
    if (file == NULL) {
        return 0;
    }
    if (fprintf(file, "operation=%s\narguments=%s\n", operation,
                arguments == NULL ? "" : arguments) < 0 || fclose(file) != 0) {
        (void)remove(OPENAI_GH_REQ_FILE);
        return 0;
    }
    (void)remove(OPENAI_GH_RESP_FILE);
    written = snprintf(command, sizeof(command), "%s %s %s", bridge,
                       OPENAI_GH_REQ_FILE, OPENAI_GH_RESP_FILE);
    if (written < 0 || (size_t)written >= sizeof(command)) {
        (void)remove(OPENAI_GH_REQ_FILE);
        return 0;
    }
    status = system(command);
    if ((status & 1) == 0) {
        (void)remove(OPENAI_GH_REQ_FILE);
        (void)remove(OPENAI_GH_RESP_FILE);
        return 0;
    }
    file = fopen(OPENAI_GH_RESP_FILE, "r");
    if (file == NULL) {
        (void)remove(OPENAI_GH_REQ_FILE);
        return 0;
    }
    used = 0U;
    result[0] = '\0';
    while (fgets(line, sizeof(line), file) != NULL) {
        size_t length = strlen(line);
        if (length >= result_size - used) {
            (void)fclose(file);
            (void)remove(OPENAI_GH_REQ_FILE);
            (void)remove(OPENAI_GH_RESP_FILE);
            return 0;
        }
        memcpy(result + used, line, length);
        used += length;
        result[used] = '\0';
    }
    (void)fclose(file);
    (void)remove(OPENAI_GH_REQ_FILE);
    (void)remove(OPENAI_GH_RESP_FILE);
    return 1;
}

static int openai_gh_prod_exec(const char *operation,
                               const char *arguments,
                               char *result, size_t result_size,
                               void *context)
{
    char first[256];
    char second[256];
    char command[OPENAI_GH_CMD_MAX];
    int written;
    int status;
    (void)context;

    if (operation == NULL || arguments == NULL || result == NULL ||
        result_size == 0U) {
        return 0;
    }
    command[0] = '\0';
    if (strcmp(operation, "check") == 0) {
        status = openai_gh_env_probe(result, result_size);
        return status > 0;
    }
    if (strcmp(operation, "fetch") == 0 || strcmp(operation, "pull") == 0 ||
        strcmp(operation, "push") == 0 || strcmp(operation, "clone") == 0) {
        status = openai_gh_env_probe(result, result_size);
        if (status <= 0) {
            return 0;
        }
        result[0] = '\0';
    }
    if (strcmp(operation, "status") == 0) {
        written = snprintf(command, sizeof(command), "GIT \"status\" \"--short\"");
    } else if (strcmp(operation, "remote") == 0) {
        written = snprintf(command, sizeof(command), "GIT \"remote\" \"-v\"");
    } else if (strcmp(operation, "fetch") == 0) {
        if (*arguments == '\0') {
            written = snprintf(command, sizeof(command), "GIT \"fetch\"");
        } else if (openai_gh_split2(arguments, first, sizeof(first), second,
                                    sizeof(second), 1) &&
                   openai_gh_token_valid(first) &&
                   (*second == '\0' || openai_gh_token_valid(second))) {
            if (*second == '\0') {
                written = snprintf(command, sizeof(command),
                                   "GIT \"fetch\" \"%s\"", first);
            } else {
                written = snprintf(command, sizeof(command),
                                   "GIT \"fetch\" \"%s\" \"%s\"", first, second);
            }
        } else {
            return 0;
        }
    } else if (strcmp(operation, "pull") == 0 ||
               strcmp(operation, "push") == 0) {
        if (!openai_gh_split2(arguments, first, sizeof(first), second,
                              sizeof(second), 0) ||
            !openai_gh_token_valid(first) || !openai_gh_token_valid(second)) {
            return 0;
        }
        written = snprintf(command, sizeof(command),
                           "GIT \"%s\" \"%s\" \"%s\"",
                           operation, first, second);
    } else if (strcmp(operation, "clone") == 0) {
        if (!openai_gh_split2(arguments, first, sizeof(first), second,
                              sizeof(second), 1) ||
            !openai_gh_url_valid(first) ||
            (*second != '\0' && !openai_gh_path_valid(second))) {
            return 0;
        }
        if (*second == '\0') {
            written = snprintf(command, sizeof(command),
                               "GIT \"clone\" \"%s\"", first);
        } else {
            written = snprintf(command, sizeof(command),
                               "GIT \"clone\" \"%s\" \"%s\"", first, second);
        }
    } else if (strcmp(operation, "issues") == 0 || strcmp(operation, "pr") == 0) {
        return openai_gh_bridge_exec(operation, arguments, result, result_size);
    } else {
        return 0;
    }
    if (written < 0 || (size_t)written >= sizeof(command)) {
        return 0;
    }
    status = system(command);
    if ((status & 1) == 0) {
        return 0;
    }
    written = snprintf(result, result_size,
                       "OpenVMS Git command completed successfully.\n");
    return written >= 0 && (size_t)written < result_size;
}

int openai_github_text(char *output, size_t output_size)
{
    int written;

    if (output == NULL || output_size == 0U) {
        return 0;
    }
    written = snprintf(output, output_size,
        "OVMS Agent GitHub integration\n"
        "-----------------------------\n"
        "AGENT/GITHUB/STATUS                local read\n"
        "AGENT/GITHUB/REMOTE                local read\n"
        "AGENT/GITHUB/CHECK                 check OpenVMS Git network prerequisites\n"
        "AGENT/GITHUB/FETCH [remote [ref]] workspace approval\n"
        "AGENT/GITHUB/PULL remote branch    workspace approval\n"
        "AGENT/GITHUB/PUSH remote branch    FULL approval\n"
        "AGENT/GITHUB/CLONE url [directory] workspace approval\n"
        "AGENT/GITHUB/ISSUES operation...   FULL approval + bridge\n"
        "AGENT/GITHUB/PR operation...       FULL approval + bridge\n"
        "\n"
        "Git transport uses the installed OpenVMS Git command.\n"
        "Native network Git requires SHARE and EXTENDED DCL parsing on non-VAX systems.\n"
        "Use AGENT/GITHUB/CHECK before FETCH, PULL, PUSH, or CLONE.\n"
        "Credentials remain Git's responsibility and are never embedded in DCL.\n"
        "Issue/PR service operations use OVMS_AGENT_GITHUB_BRIDGE.\n"
        "Only github.com HTTPS or SSH clone URLs are accepted by the native clone path.\n");
    return written >= 0 && (size_t)written < output_size;
}

int openai_gh_run_text(const char *operation,
                       const char *arguments,
                       openai_gh_executor_fn executor,
                       void *context,
                       char *output, size_t output_size)
{
    const char *args;
    char result[OPENAI_GH_RESULT_MAX];
    int required;
    int written;

    if (operation == NULL || output == NULL || output_size == 0U) {
        return 0;
    }
    args = openai_skip_ws(arguments);
    if (strlen(args) >= OPENAI_GH_ARG_MAX || !openai_gh_no_newline(args)) {
        return 0;
    }
    if (strcmp(operation, "help") == 0) {
        return openai_github_text(output, output_size);
    }
    if (strcmp(operation, "status") == 0 || strcmp(operation, "remote") == 0 ||
        strcmp(operation, "check") == 0) {
        if (*args != '\0') {
            return 0;
        }
        required = OPENAI_APPROVAL_READ;
    } else if (strcmp(operation, "fetch") == 0) {
        char first[256];
        char second[256];
        if (*args != '\0' &&
            (!openai_gh_split2(args, first, sizeof(first), second, sizeof(second), 1) ||
             !openai_gh_token_valid(first) ||
             (*second != '\0' && !openai_gh_token_valid(second)))) {
            return 0;
        }
        required = OPENAI_APPROVAL_WORK;
    } else if (strcmp(operation, "pull") == 0 || strcmp(operation, "push") == 0) {
        char first[256];
        char second[256];
        if (!openai_gh_split2(args, first, sizeof(first), second, sizeof(second), 0) ||
            !openai_gh_token_valid(first) || !openai_gh_token_valid(second)) {
            return 0;
        }
        required = strcmp(operation, "push") == 0 ?
                   OPENAI_APPROVAL_FULL : OPENAI_APPROVAL_WORK;
    } else if (strcmp(operation, "clone") == 0) {
        char first[256];
        char second[256];
        if (!openai_gh_split2(args, first, sizeof(first), second, sizeof(second), 1) ||
            !openai_gh_url_valid(first) ||
            (*second != '\0' && !openai_gh_path_valid(second))) {
            return 0;
        }
        required = OPENAI_APPROVAL_WORK;
    } else if (strcmp(operation, "issues") == 0 || strcmp(operation, "pr") == 0) {
        if (*args == '\0') {
            return 0;
        }
        required = OPENAI_APPROVAL_FULL;
    } else {
        return 0;
    }
    if (openai_policy_source() < required) {
        written = snprintf(output, output_size,
            "GitHub operation refused: %s approval policy is required.\n",
            required == OPENAI_APPROVAL_FULL ? "FULL" : "WORKSPACE");
        return written >= 0 && (size_t)written < output_size;
    }
    if (executor == NULL) {
        return 0;
    }
    result[0] = '\0';
    if (!executor(operation, args, result, sizeof(result), context)) {
        if (result[0] != '\0') {
            written = snprintf(output, output_size, "%s", result);
        } else {
            written = snprintf(output, output_size,
                "GitHub operation failed or was refused: %s\n", operation);
        }
        return written >= 0 && (size_t)written < output_size;
    }
    written = snprintf(output, output_size,
        "GitHub operation: %s\nStatus: success\n%s",
        operation, result);
    return written >= 0 && (size_t)written < output_size;
}

void openai_show_github(const char *operation, const char *arguments)
{
    char output[4096];

    if (!openai_gh_run_text(operation, arguments, openai_gh_prod_exec,
                            NULL, output, sizeof(output))) {
        if (operation != NULL && strcmp(operation, "clone") == 0) {
            (void)puts("Usage: AGENT/GITHUB/CLONE github-url [directory]");
        } else if (operation != NULL &&
                   (strcmp(operation, "pull") == 0 || strcmp(operation, "push") == 0)) {
            (void)printf("Usage: AGENT/GITHUB/%s remote branch\n",
                         strcmp(operation, "pull") == 0 ? "PULL" : "PUSH");
        } else if (operation != NULL &&
                   (strcmp(operation, "issues") == 0 || strcmp(operation, "pr") == 0)) {
            (void)printf("Usage: AGENT/GITHUB/%s operation [arguments]\n",
                         strcmp(operation, "issues") == 0 ? "ISSUES" : "PR");
        } else {
            (void)puts("Invalid GitHub operation or arguments.");
        }
        return;
    }
    (void)fputs(output, stdout);
}
