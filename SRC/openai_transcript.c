#include "openai_internal.h"
#include "command_internal.h"

#define OPENAI_TRANSCRIPT_FILE "OVMS_AGENT_TRANSCRIPT.DAT"
#define OPENAI_TRANSCRIPT_MAX 256U
#define OPENAI_TRANSCRIPT_LINE 1400U
#define OPENAI_TRANSCRIPT_ARG 768U

typedef struct openai_transcript_rec {
    char timestamp[32];
    char session[9];
    char kind[16];
    char name[32];
    char effect[16];
    char policy[16];
    char status[16];
    char arguments[OPENAI_TRANSCRIPT_ARG];
} openai_transcript_rec;

typedef struct openai_run_tool {
    const char *name;
    const char *effect;
    int minimum_policy;
    command_handler handler;
} openai_run_tool;

#define OPENAI_TOOL_READ 0
#define OPENAI_TOOL_WORK 1
#define OPENAI_TOOL_FULL 2

static const openai_run_tool openai_run_tools[] = {
    { "READ",      "read",    OPENAI_TOOL_READ, command_read },
    { "SEARCH",    "read",    OPENAI_TOOL_READ, command_search },
    { "GREP",      "read",    OPENAI_TOOL_READ, command_grep },
    { "LIST",      "read",    OPENAI_TOOL_READ, command_list },
    { "TREE",      "read",    OPENAI_TOOL_READ, command_tree },
    { "GITSTATUS", "read",    OPENAI_TOOL_READ, command_gitstatus },
    { "GITDIFF",   "read",    OPENAI_TOOL_READ, command_gitdiff },
    { "EDIT",      "write",   OPENAI_TOOL_WORK, command_edit },
    { "PATCH",     "write",   OPENAI_TOOL_WORK, command_patch },
    { "BUILD",     "execute", OPENAI_TOOL_WORK, command_build },
    { "RUN",       "execute", OPENAI_TOOL_FULL, command_run }
};

static char openai_transcript_path[256] = OPENAI_TRANSCRIPT_FILE;

static void openai_tx_now(char *output, size_t output_size)
{
    time_t now;
    struct tm *local;

    if (output == NULL || output_size == 0U) {
        return;
    }

    output[0] = '\0';
    now = time(NULL);
    local = localtime(&now);

    if (local == NULL ||
        strftime(output, output_size,
                 "%Y-%m-%dT%H:%M:%S", local) == 0U) {
        (void)strncpy(output, "1970-01-01T00:00:00",
                      output_size - 1U);
        output[output_size - 1U] = '\0';
    }
}

static void openai_tx_clean(const char *input,
                            char *output,
                            size_t output_size)
{
    size_t used;
    unsigned char ch;

    if (output == NULL || output_size == 0U) {
        return;
    }

    used = 0U;

    if (input == NULL) {
        output[0] = '\0';
        return;
    }

    while (*input != '\0' && used + 1U < output_size) {
        ch = (unsigned char)*input++;

        if (ch == (unsigned char)'|' ||
            ch == (unsigned char)'\r' ||
            ch == (unsigned char)'\n' ||
            ch == (unsigned char)'\t') {
            output[used++] = ' ';
        } else if (ch >= 32U && ch < 127U) {
            output[used++] = (char)ch;
        }
    }

    while (used > 0U && output[used - 1U] == ' ') {
        --used;
    }

    output[used] = '\0';
}

static int openai_tx_equal_ci(const char *left,
                              const char *right)
{
    unsigned char a;
    unsigned char b;

    if (left == NULL || right == NULL) {
        return 0;
    }

    while (*left != '\0' && *right != '\0') {
        a = (unsigned char)*left;
        b = (unsigned char)*right;

        if (a >= (unsigned char)'a' &&
            a <= (unsigned char)'z') {
            a = (unsigned char)(
                a - (unsigned char)'a' +
                (unsigned char)'A'
            );
        }

        if (b >= (unsigned char)'a' &&
            b <= (unsigned char)'z') {
            b = (unsigned char)(
                b - (unsigned char)'a' +
                (unsigned char)'A'
            );
        }

        if (a != b) {
            return 0;
        }

        ++left;
        ++right;
    }

    return *left == '\0' && *right == '\0';
}

static int openai_tx_policy_level(void)
{
    const char *policy;

    policy = openai_approval_name();

    if (strcmp(policy, "full") == 0) {
        return OPENAI_TOOL_FULL;
    }

    if (strcmp(policy, "workspace") == 0) {
        return OPENAI_TOOL_WORK;
    }

    return OPENAI_TOOL_READ;
}

static int openai_tx_append(const char *kind,
                            const char *name,
                            const char *effect,
                            const char *status,
                            const char *arguments)
{
    FILE *file;
    char timestamp[32];
    char session[9];
    char clean_args[OPENAI_TRANSCRIPT_ARG];

    openai_tx_now(timestamp, sizeof(timestamp));

    if (!openai_session_current_id(session)) {
        (void)strcpy(session, "--------");
    }

    openai_tx_clean(arguments, clean_args, sizeof(clean_args));

    file = fopen(openai_transcript_path, "a");

    if (file == NULL) {
        return 0;
    }

    if (fprintf(
            file,
            "%s|%s|%s|%s|%s|%s|%s|%s\n",
            timestamp,
            session,
            kind != NULL ? kind : "-",
            name != NULL ? name : "-",
            effect != NULL ? effect : "-",
            openai_approval_name(),
            status != NULL ? status : "-",
            clean_args) < 0) {
        (void)fclose(file);
        return 0;
    }

    return fclose(file) == 0;
}

static int openai_tx_parse(char *line,
                           openai_transcript_rec *record)
{
    char *field[8];
    char *position;
    unsigned int index;

    if (line == NULL || record == NULL) {
        return 0;
    }

    position = line;

    for (index = 0U; index < 8U; ++index) {
        field[index] = position;

        if (index == 7U) {
            break;
        }

        position = strchr(position, '|');

        if (position == NULL) {
            return 0;
        }

        *position++ = '\0';
    }

    position = strchr(field[7], '\n');
    if (position != NULL) {
        *position = '\0';
    }
    position = strchr(field[7], '\r');
    if (position != NULL) {
        *position = '\0';
    }

    (void)memset(record, 0, sizeof(*record));
    (void)strncpy(record->timestamp, field[0],
                  sizeof(record->timestamp) - 1U);
    (void)strncpy(record->session, field[1],
                  sizeof(record->session) - 1U);
    (void)strncpy(record->kind, field[2],
                  sizeof(record->kind) - 1U);
    (void)strncpy(record->name, field[3],
                  sizeof(record->name) - 1U);
    (void)strncpy(record->effect, field[4],
                  sizeof(record->effect) - 1U);
    (void)strncpy(record->policy, field[5],
                  sizeof(record->policy) - 1U);
    (void)strncpy(record->status, field[6],
                  sizeof(record->status) - 1U);
    (void)strncpy(record->arguments, field[7],
                  sizeof(record->arguments) - 1U);

    return 1;
}

static unsigned int openai_tx_load(openai_transcript_rec *records,
                                   unsigned int capacity)
{
    FILE *file;
    char line[OPENAI_TRANSCRIPT_LINE];
    unsigned int count;

    if (records == NULL || capacity == 0U) {
        return 0U;
    }

    file = fopen(openai_transcript_path, "r");

    if (file == NULL) {
        return 0U;
    }

    count = 0U;

    while (fgets(line, sizeof(line), file) != NULL) {
        openai_transcript_rec parsed;

        if (!openai_tx_parse(line, &parsed)) {
            continue;
        }

        if (count < capacity) {
            records[count++] = parsed;
        } else {
            unsigned int index;

            for (index = 1U; index < capacity; ++index) {
                records[index - 1U] = records[index];
            }

            records[capacity - 1U] = parsed;
        }
    }

    (void)fclose(file);
    return count;
}

static int openai_tx_safe_path(const char *path)
{
    const unsigned char *position;

    if (path == NULL || *path == '\0') {
        return 0;
    }

    if (strstr(path, "..") != NULL) {
        return 0;
    }

    position = (const unsigned char *)path;

    while (*position != (unsigned char)'\0') {
        unsigned char ch;

        ch = *position++;

        if (ch < 32U ||
            ch == (unsigned char)'*' ||
            ch == (unsigned char)'%' ||
            ch == (unsigned char)';' ||
            ch == (unsigned char)'|' ||
            ch == (unsigned char)'>' ||
            ch == (unsigned char)'<' ||
            ch == (unsigned char)'"' ||
            ch == (unsigned char)'\'' ||
            ch == (unsigned char)'/' ||
            ch == (unsigned char)'\\') {
            return 0;
        }
    }

    return 1;
}

static int openai_tx_split(const char *arguments,
                           char *first,
                           size_t first_size,
                           char *rest,
                           size_t rest_size)
{
    const char *position;
    const char *start;
    size_t length;

    if (first == NULL || first_size == 0U ||
        rest == NULL || rest_size == 0U) {
        return 0;
    }

    position = arguments != NULL ? arguments : "";

    while (*position == ' ' || *position == '\t') {
        ++position;
    }

    start = position;

    while (*position != '\0' &&
           *position != ' ' &&
           *position != '\t') {
        ++position;
    }

    length = (size_t)(position - start);

    if (length == 0U || length >= first_size) {
        return 0;
    }

    (void)memcpy(first, start, length);
    first[length] = '\0';

    while (*position == ' ' || *position == '\t') {
        ++position;
    }

    openai_tx_clean(position, rest, rest_size);
    return 1;
}

static const openai_run_tool *openai_tx_find_tool(const char *name)
{
    unsigned int index;
    unsigned int count;

    count = (unsigned int)(
        sizeof(openai_run_tools) /
        sizeof(openai_run_tools[0])
    );

    for (index = 0U; index < count; ++index) {
        if (openai_tx_equal_ci(name, openai_run_tools[index].name)) {
            return &openai_run_tools[index];
        }
    }

    return NULL;
}

void openai_tx_model_call(const char *name,
                          const char *arguments)
{
    (void)openai_tx_append(
        "model_tool", name, "model", "requested", arguments
    );
}

void openai_tx_model_result(const char *name,
                            const char *status,
                            const char *output)
{
    char bounded[OPENAI_TRANSCRIPT_ARG];

    openai_tx_clean(output, bounded, sizeof(bounded));

    (void)openai_tx_append(
        "model_tool",
        name,
        "model",
        status != NULL ? status : "result",
        bounded
    );
}

void openai_tx_loop_event(const char *name,
                          const char *status)
{
    (void)openai_tx_append(
        "loop", name, "agent", status, ""
    );
}

int openai_tool_run(agent_state *state,
                    const char *arguments)
{
    const openai_run_tool *tool;
    char name[32];
    char rest[OPENAI_TRANSCRIPT_ARG];
    int policy;

    if (state == NULL ||
        !openai_tx_split(arguments, name, sizeof(name),
                         rest, sizeof(rest))) {
        return 0;
    }

    tool = openai_tx_find_tool(name);

    if (tool == NULL) {
        (void)openai_tx_append(
            "tool", name, "unknown", "unknown", rest
        );
        return 0;
    }

    policy = openai_tx_policy_level();

    if (policy < tool->minimum_policy) {
        (void)openai_tx_append(
            "tool", tool->name, tool->effect, "denied", rest
        );
        return 0;
    }

    if (tool->minimum_policy >= OPENAI_TOOL_WORK &&
        !state->write_enabled) {
        (void)openai_tx_append(
            "tool", tool->name, tool->effect, "write-gate", rest
        );
        return 0;
    }

    if (strcmp(tool->name, "RUN") == 0 &&
        !state->dcl_enabled) {
        (void)openai_tx_append(
            "tool", tool->name, tool->effect, "dcl-gate", rest
        );
        return 0;
    }

    if (!openai_tx_append(
            "tool", tool->name, tool->effect, "dispatched", rest)) {
        return 0;
    }

    tool->handler(state, rest);
    return 1;
}

void openai_tool_run_cmd(agent_state *state,
                         const char *arguments)
{
    if (!openai_tool_run(state, arguments)) {
        (void)puts(
            "AGENT/TOOL/RUN refused or invalid. "
            "Use AGENT/TOOLS for supported capabilities."
        );
    }
}

int openai_tool_last_text(char *output,
                          size_t output_size)
{
    openai_transcript_rec records[OPENAI_TRANSCRIPT_MAX];
    unsigned int count;
    int index;
    int written;

    if (output == NULL || output_size == 0U) {
        return 0;
    }

    count = openai_tx_load(records, OPENAI_TRANSCRIPT_MAX);

    for (index = (int)count - 1; index >= 0; --index) {
        if (strcmp(records[index].kind, "tool") == 0 ||
            strcmp(records[index].kind, "model_tool") == 0) {
            written = snprintf(
                output, output_size,
                "OVMS Agent last tool call\n"
                "-------------------------\n"
                "Time:     %s\n"
                "Session:  %s\n"
                "Tool:     %s\n"
                "Effect:   %s\n"
                "Policy:   %s\n"
                "Status:   %s\n"
                "Args:     %s\n",
                records[index].timestamp,
                records[index].session,
                records[index].name,
                records[index].effect,
                records[index].policy,
                records[index].status,
                records[index].arguments
            );

            return written >= 0 &&
                   (size_t)written < output_size;
        }
    }

    written = snprintf(
        output, output_size,
        "OVMS Agent last tool call\n"
        "-------------------------\n"
        "No persisted tool calls.\n"
    );

    return written >= 0 && (size_t)written < output_size;
}

int openai_tool_hist_text(char *output,
                          size_t output_size)
{
    openai_transcript_rec records[OPENAI_TRANSCRIPT_MAX];
    unsigned int count;
    unsigned int shown;
    int index;
    size_t used;
    int written;

    if (output == NULL || output_size == 0U) {
        return 0;
    }

    count = openai_tx_load(records, OPENAI_TRANSCRIPT_MAX);

    written = snprintf(
        output, output_size,
        "OVMS Agent tool history\n"
        "-----------------------\n"
    );

    if (written < 0 || (size_t)written >= output_size) {
        return 0;
    }

    used = (size_t)written;
    shown = 0U;

    for (index = (int)count - 1;
         index >= 0 && shown < 20U;
         --index) {
        if (strcmp(records[index].kind, "tool") != 0 &&
            strcmp(records[index].kind, "model_tool") != 0) {
            continue;
        }

        ++shown;

        written = snprintf(
            output + used, output_size - used,
            "%2u %s %s %-10s %-10s %s\n",
            shown,
            records[index].timestamp,
            records[index].session,
            records[index].name,
            records[index].status,
            records[index].arguments
        );

        if (written < 0 ||
            (size_t)written >= output_size - used) {
            return 0;
        }

        used += (size_t)written;
    }

    written = snprintf(
        output + used, output_size - used,
        "Entries shown: %u\n",
        shown
    );

    return written >= 0 &&
           (size_t)written < output_size - used;
}

void openai_show_tool_last(void)
{
    char output[4096];

    if (!openai_tool_last_text(output, sizeof(output))) {
        (void)puts("Unable to show last tool call.");
        return;
    }

    (void)fputs(output, stdout);
}

void openai_show_tool_hist(void)
{
    char output[16384];

    if (!openai_tool_hist_text(output, sizeof(output))) {
        (void)puts("Unable to show tool history.");
        return;
    }

    (void)fputs(output, stdout);
}

int openai_tool_clear(const char *arguments)
{
    const char *required;
    const char *value;

    required = "CLEAR TOOL HISTORY";
    value = arguments != NULL ? arguments : "";

    while (*value == ' ' || *value == '\t') {
        ++value;
    }

    if (strcmp(value, required) != 0) {
        return 0;
    }

    while (remove(openai_transcript_path) == 0) {
    }

    return 1;
}

void openai_tool_clear_cmd(const char *arguments)
{
    if (!openai_tool_clear(arguments)) {
        (void)puts(
            "Usage: AGENT/TOOL/CLEAR CLEAR TOOL HISTORY"
        );
        return;
    }

    (void)puts("Tool and session transcript history cleared.");
}


static int openai_tx_is_result(const openai_transcript_rec *record)
{
    if (record == NULL) return 0;
    return openai_tx_equal_ci(record->kind, "model_tool") &&
           strncmp(record->arguments, "TOOL RESULT", 11U) == 0;
}

int openai_session_results_text(const char *arguments,
                                char *output,
                                size_t output_size)
{
    openai_transcript_rec records[OPENAI_TRANSCRIPT_MAX];
    char session[9];
    unsigned int count, index, shown;
    size_t used;
    int written;

    if (output == NULL || output_size == 0U) return 0;
    openai_tx_clean(arguments, session, sizeof(session));
    if (strlen(session) != 8U) return 0;

    count = openai_tx_load(records, OPENAI_TRANSCRIPT_MAX);
    written = snprintf(
        output, output_size,
        "OVMS Agent normalized session results\n"
        "-------------------------------------\n"
        "Session: %s\n", session);
    if (written < 0 || (size_t)written >= output_size) return 0;

    used = (size_t)written;
    shown = 0U;

    for (index = 0U; index < count; ++index) {
        if (!openai_tx_equal_ci(records[index].session, session) ||
            !openai_tx_is_result(&records[index])) continue;

        ++shown;
        written = snprintf(
            output + used, output_size - used,
            "%s tool=%s status=%s %s\n",
            records[index].timestamp,
            records[index].name,
            records[index].status,
            records[index].arguments);

        if (written < 0 ||
            (size_t)written >= output_size - used) return 0;
        used += (size_t)written;
    }

    written = snprintf(
        output + used, output_size - used,
        "Results: %u\n", shown);

    return written >= 0 &&
           (size_t)written < output_size - used;
}

int openai_session_result_last(const char *arguments,
                               char *output,
                               size_t output_size)
{
    openai_transcript_rec records[OPENAI_TRANSCRIPT_MAX];
    char session[9];
    unsigned int count, index;
    int written;

    if (output == NULL || output_size == 0U) return 0;
    openai_tx_clean(arguments, session, sizeof(session));
    if (strlen(session) != 8U) return 0;

    count = openai_tx_load(records, OPENAI_TRANSCRIPT_MAX);

    for (index = count; index > 0U; --index) {
        openai_transcript_rec *record = &records[index - 1U];

        if (!openai_tx_equal_ci(record->session, session) ||
            !openai_tx_is_result(record)) continue;

        written = snprintf(
            output, output_size,
            "OVMS Agent last normalized result\n"
            "---------------------------------\n"
            "Session: %s\n"
            "%s tool=%s status=%s\n"
            "%s\n",
            session,
            record->timestamp,
            record->name,
            record->status,
            record->arguments);

        return written >= 0 &&
               (size_t)written < output_size;
    }

    written = snprintf(
        output, output_size,
        "OVMS Agent last normalized result\n"
        "---------------------------------\n"
        "Session: %s\n"
        "No normalized results found.\n",
        session);

    return written >= 0 &&
           (size_t)written < output_size;
}

int openai_session_clear_results(const char *arguments)
{
    openai_transcript_rec records[OPENAI_TRANSCRIPT_MAX];
    char session[9];
    char temp_path[OPENAI_TRANSCRIPT_PATH_MAX];
    unsigned int count, index, removed;
    FILE *file;

    openai_tx_clean(arguments, session, sizeof(session));
    if (strlen(session) != 8U) return 0;

    count = openai_tx_load(records, OPENAI_TRANSCRIPT_MAX);

    if (snprintf(temp_path, sizeof(temp_path),
                 "%s.M239", openai_transcript_path) < 0) return 0;

    while (remove(temp_path) == 0) {
    }

    file = fopen(temp_path, "w");
    if (file == NULL) return 0;

    removed = 0U;

    for (index = 0U; index < count; ++index) {
        if (openai_tx_equal_ci(records[index].session, session) &&
            openai_tx_is_result(&records[index])) {
            ++removed;
            continue;
        }

        if (fprintf(
                file,
                "%s|%s|%s|%s|%s|%s|%s|%s\n",
                records[index].timestamp,
                records[index].session,
                records[index].kind,
                records[index].name,
                records[index].effect,
                records[index].policy,
                records[index].status,
                records[index].arguments) < 0) {
            (void)fclose(file);
            (void)remove(temp_path);
            return 0;
        }
    }

    if (fclose(file) != 0) {
        (void)remove(temp_path);
        return 0;
    }

    while (remove(openai_transcript_path) == 0) {
    }

    if (rename(temp_path, openai_transcript_path) != 0) {
        (void)remove(temp_path);
        return 0;
    }

    return removed > 0U;
}

void openai_show_session_results(const char *arguments)
{
    char output[32768];

    if (!openai_session_results_text(
            arguments, output, sizeof(output))) {
        (void)puts("Unable to show normalized session results.");
        return;
    }
    (void)fputs(output, stdout);
}

void openai_show_session_result(const char *arguments)
{
    char output[8192];

    if (!openai_session_result_last(
            arguments, output, sizeof(output))) {
        (void)puts("Unable to show last normalized session result.");
        return;
    }
    (void)fputs(output, stdout);
}

void openai_session_clear_res_cmd(const char *arguments)
{
    if (openai_session_clear_results(arguments)) {
        (void)puts("Normalized session results cleared.");
    } else {
        (void)puts("No normalized session results were cleared.");
    }
}

int openai_session_hist_text(const char *arguments,
                             char *output,
                             size_t output_size)
{
    openai_transcript_rec records[OPENAI_TRANSCRIPT_MAX];
    char session[9];
    unsigned int count;
    unsigned int shown;
    unsigned int index;
    size_t used;
    int written;

    if (output == NULL || output_size == 0U) {
        return 0;
    }

    openai_tx_clean(arguments, session, sizeof(session));

    if (strlen(session) != 8U) {
        return 0;
    }

    count = openai_tx_load(records, OPENAI_TRANSCRIPT_MAX);

    written = snprintf(
        output, output_size,
        "OVMS Agent session transcript\n"
        "-----------------------------\n"
        "Session: %s\n",
        session
    );

    if (written < 0 || (size_t)written >= output_size) {
        return 0;
    }

    used = (size_t)written;
    shown = 0U;

    for (index = 0U; index < count; ++index) {
        if (!openai_tx_equal_ci(
                records[index].session, session)) {
            continue;
        }

        ++shown;

        written = snprintf(
            output + used, output_size - used,
            "%s kind=%s name=%s effect=%s "
            "policy=%s status=%s args=%s\n",
            records[index].timestamp,
            records[index].kind,
            records[index].name,
            records[index].effect,
            records[index].policy,
            records[index].status,
            records[index].arguments
        );

        if (written < 0 ||
            (size_t)written >= output_size - used) {
            return 0;
        }

        used += (size_t)written;
    }

    written = snprintf(
        output + used, output_size - used,
        "Entries: %u\n",
        shown
    );

    return written >= 0 &&
           (size_t)written < output_size - used;
}

int openai_session_export(const char *arguments)
{
    char session[32];
    char path[256];
    char output[32768];
    FILE *probe;
    FILE *file;

    if (!openai_tx_split(
            arguments, session, sizeof(session),
            path, sizeof(path))) {
        return 0;
    }

    if (strlen(session) != 8U ||
        !openai_tx_safe_path(path)) {
        return 0;
    }

    probe = fopen(path, "r");

    if (probe != NULL) {
        (void)fclose(probe);
        return 0;
    }

    if (!openai_session_hist_text(
            session, output, sizeof(output))) {
        return 0;
    }

    file = fopen(path, "w");

    if (file == NULL) {
        return 0;
    }

    if (fputs(output, file) == EOF) {
        (void)fclose(file);
        return 0;
    }

    return fclose(file) == 0;
}

void openai_session_export_cmd(const char *arguments)
{
    if (!openai_session_export(arguments)) {
        (void)puts(
            "Usage: AGENT/SESSION/EXPORT <id> <new-safe-filespec>"
        );
        return;
    }

    (void)puts("Session transcript exported.");
}

void openai_show_session_hist(const char *arguments)
{
    char output[32768];

    if (!openai_session_hist_text(
            arguments, output, sizeof(output))) {
        (void)puts("Usage: AGENT/SESSION/HISTORY <id>");
        return;
    }

    (void)fputs(output, stdout);
}

int openai_session_exec(agent_state *state,
                        const char *arguments)
{
    char session[32];
    char goal[OPENAI_TRANSCRIPT_ARG];

    if (state == NULL ||
        !openai_tx_split(
            arguments, session, sizeof(session),
            goal, sizeof(goal)) ||
        strlen(session) != 8U ||
        goal[0] == '\0') {
        return 0;
    }

    if (!openai_session_resume(session)) {
        return 0;
    }

    (void)openai_tx_append(
        "exec", "SESSION", "agent", "started", goal
    );
    openai_exec_context(state, goal);
    return 1;
}

void openai_session_exec_cmd(agent_state *state,
                             const char *arguments)
{
    if (!openai_session_exec(state, arguments)) {
        (void)puts(
            "Usage: AGENT/SESSION/EXEC <id> <goal>"
        );
    }
}

int openai_exec_resume(agent_state *state,
                       const char *goal)
{
    char session[9];

    if (state == NULL ||
        goal == NULL ||
        *goal == '\0' ||
        !openai_session_current_id(session)) {
        return 0;
    }

    (void)openai_tx_append(
        "exec", "RESUME", "agent", "started", goal
    );
    openai_exec_context(state, goal);
    return 1;
}

void openai_exec_resume_cmd(agent_state *state,
                            const char *goal)
{
    if (!openai_exec_resume(state, goal)) {
        (void)puts(
            "AGENT/EXEC/RESUME requires a current session and goal."
        );
    }
}

int openai_exec_fork(agent_state *state,
                     const char *arguments)
{
    const char *divider;
    char current[9];
    char fork_args[256];
    char new_id[9];
    char name[128];
    char goal[OPENAI_TRANSCRIPT_ARG];
    size_t name_length;

    if (state == NULL || arguments == NULL ||
        !openai_session_current_id(current)) {
        return 0;
    }

    divider = strstr(arguments, "::");

    if (divider == NULL) {
        return 0;
    }

    name_length = (size_t)(divider - arguments);

    while (name_length > 0U &&
           (arguments[name_length - 1U] == ' ' ||
            arguments[name_length - 1U] == '\t')) {
        --name_length;
    }

    while (*arguments == ' ' || *arguments == '\t') {
        ++arguments;
        --name_length;
    }

    if (name_length == 0U || name_length >= sizeof(name)) {
        return 0;
    }

    (void)memcpy(name, arguments, name_length);
    name[name_length] = '\0';

    divider += 2;

    while (*divider == ' ' || *divider == '\t') {
        ++divider;
    }

    openai_tx_clean(divider, goal, sizeof(goal));

    if (goal[0] == '\0') {
        return 0;
    }

    (void)snprintf(
        fork_args, sizeof(fork_args),
        "%s %s", current, name
    );

    if (!openai_session_fork(fork_args, new_id)) {
        return 0;
    }

    (void)openai_tx_append(
        "exec", "FORK", "agent", "started", goal
    );
    openai_exec_context(state, goal);
    return 1;
}

void openai_exec_fork_cmd(agent_state *state,
                          const char *arguments)
{
    if (!openai_exec_fork(state, arguments)) {
        (void)puts(
            "Usage: AGENT/EXEC/FORK <new-name> :: <goal>"
        );
    }
}

void openai_tx_note_exec(const char *name,
                         const char *goal,
                         const char *status)
{
    (void)openai_tx_append(
        "exec", name, "agent", status, goal
    );
}

void openai_test_tx_path(const char *path)
{
    if (path == NULL || *path == '\0') {
        (void)strcpy(
            openai_transcript_path,
            OPENAI_TRANSCRIPT_FILE
        );
        return;
    }

    (void)strncpy(
        openai_transcript_path,
        path,
        sizeof(openai_transcript_path) - 1U
    );
    openai_transcript_path[
        sizeof(openai_transcript_path) - 1U
    ] = '\0';
}
