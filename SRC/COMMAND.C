#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "command.h"
#include "project.h"
#include "util.h"
#include "edit.h"
#include "openai.h"

typedef void (*command_handler)(agent_state *state,
                                const char *arguments);

typedef struct command_entry {
    const char *name;
    const char *description;
    command_handler handler;
} command_entry;

static int command_decode_escapes(char *text);
static void command_help(agent_state *state, const char *arguments);
static void command_version(agent_state *state, const char *arguments);
static void command_root(agent_state *state, const char *arguments);
static void command_status(agent_state *state, const char *arguments);
static char *command_next_argument(char **cursor);
static void command_list(agent_state *state, const char *arguments);
static void command_edit(agent_state *state, const char *arguments);
static void command_read(agent_state *state, const char *arguments);
static void command_gitstatus(agent_state *state,  const char *arguments);
static void command_gitdiff(agent_state *state, const char *arguments);
static void command_build(agent_state *state, const char *arguments);
static void command_quit(agent_state *state, const char *arguments);
static void command_patch(agent_state *state, const char *arguments);
static void command_search(agent_state *state, const char *arguments);
static void command_tree(agent_state *state, const char *arguments);
static void command_ask(agent_state *state, const char *arguments);
static void command_chat(agent_state *state,
                         const char *arguments);
static void command_chat_reset(agent_state *state,
                               const char *arguments);
static void command_review(agent_state *state,
                           const char *arguments);
static void command_agent(agent_state *state,
                          const char *arguments);


static const command_entry command_table[] = {
    { "HELP",    "Display command help", command_help },
    { "VERSION", "Display agent version", command_version },
    { "ROOT",    "Display the project root", command_root },
    { "STATUS",  "Display agent status", command_status },
    { "LIST", "List a directory: LIST [path]", command_list },
    { "TREE", "Display directory tree: TREE [path]", command_tree },
    { "READ", "Read lines: READ file [start [count]]", command_read },
    { "BUILD", "Build the current project", command_build },
    { "GITSTATUS", "Display Git status", command_gitstatus },
    { "GITDIFF", "Display uncommitted source changes", command_gitdiff },
    { "EDIT", "Edit a file: EDIT file", command_edit },
{ "CHAT", "Continue an OpenAI conversation: CHAT prompt", command_chat },
{ "CHAT/RESET", "Reset the OpenAI conversation", command_chat_reset },
{ "REVIEW", "Review source with OpenAI: REVIEW file", command_review },
{ "AGENT", "Run read-only AI agent: AGENT goal", command_agent },
    { "ASK", "Send a prompt to OpenAI: ASK prompt", command_ask },
    { "SEARCH", "Search a file: SEARCH file \"text\"", command_search },
    { "PATCH", "Replace exact text: PATCH file \"old\" \"new\"", command_patch },
    { "QUIT",    "Exit OVMS Agent", command_quit },
    { "EXIT",    "Exit OVMS Agent", command_quit },
    { NULL, NULL, NULL }
};

void command_prompt(void)
{
    (void)fputs("OVMS-AGENT> ", stdout);
    (void)fflush(stdout);
}

void command_execute(agent_state *state, char *input)
{
    const command_entry *entry;
    char *command;
    char *arguments;
    char *position;

    if (state == NULL || input == NULL) {
        return;
    }

    util_trim(input);
    command = util_skip_space(input);

    if (command == NULL || *command == '\0') {
        return;
    }

    position = command;

    while (*position != '\0' && *position != ' ' && *position != '\t') {
        ++position;
    }

    if (*position != '\0') {
        *position = '\0';
        arguments = util_skip_space(position + 1);
    } else {
        arguments = position;
    }

    util_uppercase(command);

    for (entry = command_table; entry->name != NULL; ++entry) {
        if (strcmp(command, entry->name) == 0) {
            entry->handler(state, arguments);
            return;
        }
    }

    if (strcmp(command, "LIST/ALL") == 0) {
        project_list(state, arguments,  1);
        return;
    }

    (void)printf("Unknown command: %s\n", command);
    (void)puts("Enter HELP for available commands.");
}

     static void command_edit(agent_state *state,
                              const char *arguments)
     {
         edit_run(state, arguments);
     }

static void command_agent(agent_state *state,
                          const char *arguments)
{
    openai_agent(state, arguments);
}


static void command_chat(agent_state *state,
                         const char *arguments)
{
    openai_chat(state, arguments);
}

static void command_review(agent_state *state,
                           const char *arguments)
{
    openai_review_file(state, arguments);
}

static void command_chat_reset(agent_state *state,
                               const char *arguments)
{
    (void)state;
    (void)arguments;
    openai_chat_reset();
}

static void command_ask(agent_state *state,
                        const char *arguments)
{
    openai_ask(state, arguments);
}

static void command_build(agent_state *state,
                          const char *arguments)
{
    (void)arguments;
    project_build(state);
}

static void command_tree(agent_state *state,
                         const char *arguments)
{
    project_tree(state, arguments);
}

static void command_search(agent_state *state,
                           const char *arguments)
{
    char work[OVMS_AGENT_INPUT_SIZE];
    char *cursor;
    char *path;
    char *pattern;
    char *extra;

    if (arguments == NULL || *arguments == '\0') {
        (void)puts("Usage: SEARCH file \"text\"");
        return;
    }


    if (strlen(arguments) >= sizeof(work)) {
        (void)puts("Search arguments are too long.");
        return;
    }

    (void)strcpy(work, arguments);
    cursor = work;

    path = command_next_argument(&cursor);
    pattern = command_next_argument(&cursor);
    extra = command_next_argument(&cursor);

    if (path == NULL ||
        pattern == NULL ||
        extra != NULL) {
        (void)puts("Usage: SEARCH file \"text\"");
        return;
    }
if (!command_decode_escapes(pattern)) {
    (void)puts("Invalid escape sequence in search text.");
    return;
}


    project_search(state, path, pattern);
}

static void command_gitdiff(agent_state *state,
                            const char *arguments)
{
    (void)arguments;
    project_git_diff(state);
}

static void command_help(agent_state *state, const char *arguments)
{
    const command_entry *entry;

    (void)state;
    (void)arguments;
    (void)puts("Commands:");

    for (entry = command_table; entry->name != NULL; ++entry) {
        if (strcmp(entry->name, "EXIT") != 0) {
            (void)printf("  %-10s %s\n",
                         entry->name,
                         entry->description);
        }
    }
}

static void command_version(agent_state *state,
                            const char *arguments)
{
    (void)state;
    (void)arguments;
    (void)printf("OVMS Agent %s\n", OVMS_AGENT_VERSION);
}

static void command_root(agent_state *state, const char *arguments)
{
    (void)arguments;
    project_show_root(state);
}

static void command_status(agent_state *state, const char *arguments)
{
    (void)arguments;
    project_show_status(state);
}

static void command_gitstatus(agent_state *state,
                              const char *arguments)
{
    (void)arguments;
    project_git_status(state);
}

static void command_list(agent_state *state,
                         const char *arguments)
{
    project_list(state, arguments, 0);
}

static void command_read(agent_state *state,
                         const char *arguments)
{
    char work[OVMS_AGENT_INPUT_SIZE];
    char *cursor;
    char *path;
    char *start_text;
    char *count_text;
    char *extra;
    char *end;
    unsigned long start_line;
    unsigned long line_count;

    if (arguments == NULL || *arguments == '\0') {
        (void)puts(
            "Usage: READ file [start_line [line_count]]"
        );
        return;
    }

    if (strlen(arguments) >= sizeof(work)) {
        (void)puts("READ arguments are too long.");
        return;
    }

    (void)strcpy(work, arguments);
    cursor = work;

    path = command_next_argument(&cursor);
    start_text = command_next_argument(&cursor);
    count_text = command_next_argument(&cursor);
    extra = command_next_argument(&cursor);

    if (path == NULL || extra != NULL) {
        (void)puts(
            "Usage: READ file [start_line [line_count]]"
        );
        return;
    }

    start_line = 1UL;
    line_count = 0UL;

    if (start_text != NULL) {
        start_line = strtoul(start_text, &end, 10);

        if (*start_text == '\0' ||
            *end != '\0' ||
            start_line == 0UL) {
            (void)puts("Start line must be a positive integer.");
            return;
        }
    }

    if (count_text != NULL) {
        line_count = strtoul(count_text, &end, 10);

        if (*count_text == '\0' ||
            *end != '\0' ||
            line_count == 0UL) {
            (void)puts("Line count must be a positive integer.");
            return;
        }
    }

    project_read(state,
                 path,
                 start_line,
                 line_count);
}

static int command_decode_escapes(char *text)
{
    char *source;
    char *destination;

    if (text == NULL) {
        return 0;
    }

    source = text;
    destination = text;

    while (*source != '\0') {
        if ((unsigned char)*source != 92U) {
            *destination++ = *source++;
            continue;
        }

        ++source;

        if (*source == '\0') {
            return 0;
        }

        switch (*source) {
        case 'n':
            *destination++ = '\n';
            break;

        case 'r':
            *destination++ = '\r';
            break;

        case 't':
            *destination++ = '\t';
            break;

        case '"':
            *destination++ = '"';
            break;

        default:
            if ((unsigned char)*source == 92U) {
                *destination++ = (char)92;
            } else {
                return 0;
            }
            break;
        }

        ++source;
    }

    *destination = '\0';
    return 1;
}

static char *command_next_argument(char **cursor)
{
    char *start;
    char *end;

    if (cursor == NULL || *cursor == NULL) {
        return NULL;
    }

    start = util_skip_space(*cursor);

    if (start == NULL || *start == '\0') {
        *cursor = start;
        return NULL;
    }

    if ((unsigned char)*start == 34U) {
    ++start;
    end = start;

    while (*end != '\0') {
        if ((unsigned char)*end == 92U &&
            end[1] != '\0') {
            end += 2;
            continue;
        }

        if ((unsigned char)*end == 34U) {
            break;
        }

        ++end;
    }

    if ((unsigned char)*end != 34U) {
        return NULL;
    }

    *end = '\0';
    *cursor = end + 1;
    return start;
}

    end = start;

    while (*end != '\0' &&
           *end != ' ' &&
           *end != '\t') {
        ++end;
    }

    if (*end != '\0') {
        *end = '\0';
        *cursor = end + 1;
    } else {
        *cursor = end;
    }

    return start;
}

static void command_patch(agent_state *state,
                          const char *arguments)
{
    char work[OVMS_AGENT_INPUT_SIZE];
    char *cursor;
    char *path;
    char *old_text;
    char *new_text;
    char *extra;

    if (arguments == NULL || *arguments == '\0') {
        (void)puts(
            "Usage: PATCH file \"old text\" \"new text\""
        );
        return;
    }

    if (strlen(arguments) >= sizeof(work)) {
        (void)puts("Patch arguments are too long.");
        return;
    }

    (void)strcpy(work, arguments);
    cursor = work;

    path = command_next_argument(&cursor);
    old_text = command_next_argument(&cursor);
    new_text = command_next_argument(&cursor);
    extra = command_next_argument(&cursor);

    if (path == NULL ||
        old_text == NULL ||
        new_text == NULL ||
        extra != NULL) {
        (void)puts(
            "Usage: PATCH file \"old text\" \"new text\""
        );
        (void)puts(
            "Quotes are required when text contains spaces."
        );
        return;
    }


if (!command_decode_escapes(old_text) ||
    !command_decode_escapes(new_text)) {
    (void)puts("Invalid escape sequence in patch text.");
    return;
}

    (void)project_patch(state,
                        path,
                        old_text,
                        new_text);

}


static void command_quit(agent_state *state, const char *arguments)
{
    (void)arguments;
    agent_stop(state);
}
