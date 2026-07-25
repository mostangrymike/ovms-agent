#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "command_internal.h"
#include "edit.h"
#include "project.h"
#include "util.h"

static const command_entry project_commands[] = {
    { "ROOT", "Display the project root", command_root },
    { "STATUS", "Display agent status", command_status },
    { "LIST", "List a directory: LIST [path]", command_list },
    { "TREE", "Display directory tree: TREE [path]", command_tree },
    { "READ", "Read lines: READ file [start [count]]", command_read },
    { "BUILD", "Build the current project", command_build },
    { "GITSTATUS", "Display Git status", command_gitstatus },
    { "GITDIFF", "Display uncommitted source changes", command_gitdiff },
    { "EDIT", "Edit a file: EDIT file", command_edit },
    { "SEARCH", "Search a file: SEARCH file \"text\"", command_search },
    { "PATCH", "Replace exact text: PATCH file \"old\" \"new\"", command_patch }
};

void command_register_project(void)
{
    (void)command_registry_add(
        project_commands,
        sizeof(project_commands) / sizeof(project_commands[0])
    );
}

void command_build(agent_state *state,
                   const char *arguments)
{
    (void)arguments;
    project_build(state);
}

void command_tree(agent_state *state,
                  const char *arguments)
{
    project_tree(state, arguments);
}

void command_gitdiff(agent_state *state,
                     const char *arguments)
{
    (void)arguments;
    project_git_diff(state);
}

void command_root(agent_state *state,
                  const char *arguments)
{
    (void)arguments;
    project_show_root(state);
}

void command_status(agent_state *state,
                    const char *arguments)
{
    (void)arguments;
    project_show_status(state);
}

void command_gitstatus(agent_state *state,
                       const char *arguments)
{
    (void)arguments;
    project_git_status(state);
}

void command_list(agent_state *state,
                  const char *arguments)
{
    project_list(state, arguments, 0);
}

void command_edit(agent_state *state,
                  const char *arguments)
{
    edit_run(state, arguments);
}

void command_search(agent_state *state,
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

void command_read(agent_state *state,
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

    project_read(state, path, start_line, line_count);
}

void command_patch(agent_state *state,
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

    (void)project_patch(state, path, old_text, new_text);
}
