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

    return 1;
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
