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
    { "GREP", "Search project files: GREP \"text\" [path] [/CONTEXT=n] [/LIMIT=n] [/CASE=value] [/COUNT] [/FILES] "
            "[/NAME=pattern] [/EXCLUDE=pattern] [/DEPTH=n]", command_grep },
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

void command_grep(agent_state *state,
                  const char *arguments)
{
    char work[OVMS_AGENT_INPUT_SIZE];
    char *cursor;
    char *pattern;
    char *items[9];
    char *path;
    char *name_pattern;
    char *exclude_pattern;
    char *end;
    unsigned long context_value;
    unsigned long limit_value;
    unsigned long depth_value;
    unsigned int context_seen;
    unsigned int limit_seen;
    unsigned int case_seen;
    unsigned int count_seen;
    unsigned int files_seen;
    unsigned int name_seen;
    unsigned int exclude_seen;
    unsigned int depth_seen;
    unsigned int item_count;
    unsigned int index;
    int case_sensitive;
    int count_only;
    int files_only;

    if (arguments == NULL || *arguments == '\0') {
        (void)puts(
            "Usage: GREP \"text\" [path] "
            "[/CONTEXT=n] [/LIMIT=n] [/CASE=value] [/COUNT] [/FILES] "
            "[/NAME=pattern] [/EXCLUDE=pattern] [/DEPTH=n]"
        );
        return;
    }

    if (strlen(arguments) >= sizeof(work)) {
        (void)puts("GREP arguments are too long.");
        return;
    }

    (void)strcpy(work, arguments);
    cursor = work;

    pattern = command_next_argument(&cursor);
    item_count = 0U;

    while (item_count < 9U) {
        items[item_count] =
            command_next_argument(&cursor);

        if (items[item_count] == NULL) {
            break;
        }

        ++item_count;
    }

    if (pattern == NULL ||
        *pattern == '\0' ||
        command_next_argument(&cursor) != NULL) {
        (void)puts(
            "Usage: GREP \"text\" [path] "
            "[/CONTEXT=n] [/LIMIT=n] [/CASE=value] [/COUNT] [/FILES] "
            "[/NAME=pattern] [/EXCLUDE=pattern] [/DEPTH=n]"
        );
        return;
    }

    path = NULL;
    name_pattern = NULL;
    exclude_pattern = NULL;
    context_value = 0UL;
    limit_value = 100UL;
    depth_value = 16UL;
    case_sensitive = 0;
    count_only = 0;
    files_only = 0;
    context_seen = 0U;
    limit_seen = 0U;
    case_seen = 0U;
    count_seen = 0U;
    files_seen = 0U;
    name_seen = 0U;
    exclude_seen = 0U;
    depth_seen = 0U;

    for (index = 0U; index < item_count; ++index) {
        char *item;

        item = items[index];

        if (item[0] != '/') {
            if (path != NULL ||
                context_seen ||
                limit_seen ||
                case_seen ||
                count_seen ||
                files_seen ||
                name_seen ||
                exclude_seen ||
                depth_seen) {
                (void)puts(
                    "GREP path must precede qualifiers."
                );
                return;
            }

            path = item;
            continue;
        }

        util_uppercase(item);

        if (strncmp(item,
                    "/CONTEXT=",
                    9U) == 0) {
            if (context_seen) {
                (void)puts(
                    "Duplicate /CONTEXT qualifier."
                );
                return;
            }

            context_value = strtoul(item + 9,
                                   &end,
                                   10);

            if (end == item + 9 ||
                *end != '\0' ||
                context_value > 20UL) {
                (void)puts(
                    "Context must be an integer "
                    "from 0 to 20."
                );
                return;
            }

            context_seen = 1U;
        } else if (strncmp(item,
                           "/LIMIT=",
                           7U) == 0) {
            if (limit_seen) {
                (void)puts(
                    "Duplicate /LIMIT qualifier."
                );
                return;
            }

            limit_value = strtoul(item + 7,
                                 &end,
                                 10);

            if (end == item + 7 ||
                *end != '\0' ||
                limit_value == 0UL ||
                limit_value > 100UL) {
                (void)puts(
                    "Limit must be an integer "
                    "from 1 to 100."
                );
                return;
            }

            limit_seen = 1U;
        } else if (strncmp(item,
                           "/CASE=",
                           6U) == 0) {
            if (case_seen) {
                (void)puts(
                    "Duplicate /CASE qualifier."
                );
                return;
            }

            if (strcmp(item + 6,
                       "SENSITIVE") == 0) {
                case_sensitive = 1;
            } else if (strcmp(item + 6,
                              "INSENSITIVE") == 0) {
                case_sensitive = 0;
            } else {
                (void)puts(
                    "Case must be SENSITIVE "
                    "or INSENSITIVE."
                );
                return;
            }

            case_seen = 1U;
        } else if (strcmp(item, "/COUNT") == 0) {
            if (count_seen) {
                (void)puts(
                    "Duplicate /COUNT qualifier."
                );
                return;
            }

            count_only = 1;
            count_seen = 1U;
        } else if (strncmp(item,
                           "/COUNT=",
                           7U) == 0) {
            (void)puts(
                "/COUNT does not take a value."
            );
            return;
        } else if (strcmp(item, "/FILES") == 0) {
            if (files_seen) {
                (void)puts(
                    "Duplicate /FILES qualifier."
                );
                return;
            }

            files_only = 1;
            files_seen = 1U;
        } else if (strncmp(item,
                           "/FILES=",
                           7U) == 0) {
            (void)puts(
                "/FILES does not take a value."
            );
            return;
        } else if (strncmp(item,
                           "/NAME=",
                           6U) == 0) {
            if (name_seen) {
                (void)puts(
                    "Duplicate /NAME qualifier."
                );
                return;
            }

            if (item[6] == '\0') {
                (void)puts(
                    "/NAME requires a wildcard pattern."
                );
                return;
            }

            name_pattern = item + 6;
            name_seen = 1U;
        } else if (strcmp(item, "/NAME") == 0) {
            (void)puts(
                "/NAME requires a wildcard pattern."
            );
            return;
        } else if (strncmp(item,
                           "/EXCLUDE=",
                           9U) == 0) {
            if (exclude_seen) {
                (void)puts(
                    "Duplicate /EXCLUDE qualifier."
                );
                return;
            }

            if (item[9] == '\0') {
                (void)puts(
                    "/EXCLUDE requires a wildcard pattern."
                );
                return;
            }

            exclude_pattern = item + 9;
            exclude_seen = 1U;
        } else if (strcmp(item, "/EXCLUDE") == 0) {
            (void)puts(
                "/EXCLUDE requires a wildcard pattern."
            );
            return;
        } else if (strncmp(item,
                           "/DEPTH=",
                           7U) == 0) {
            if (depth_seen) {
                (void)puts(
                    "Duplicate /DEPTH qualifier."
                );
                return;
            }

            depth_value = strtoul(item + 7,
                                 &end,
                                 10);

            if (end == item + 7 ||
                *end != '\0' ||
                depth_value > 16UL) {
                (void)puts(
                    "Depth must be an integer "
                    "from 0 to 16."
                );
                return;
            }

            depth_seen = 1U;
        } else if (strcmp(item, "/DEPTH") == 0) {
            (void)puts(
                "/DEPTH requires an integer value."
            );
            return;
        } else {
            (void)puts(
                "Only /CONTEXT=n, /LIMIT=n, /CASE=value, "
                "/COUNT, /FILES, /NAME=pattern, "
                "/EXCLUDE=pattern, and /DEPTH=n are supported."
            );
            return;
        }
    }

    if (count_seen && context_value != 0UL) {
        (void)puts(
            "/COUNT is not compatible with /CONTEXT."
        );
        return;
    }

    if (files_seen && context_value != 0UL) {
        (void)puts(
            "/FILES is not compatible with /CONTEXT."
        );
        return;
    }

    if (files_seen && count_seen) {
        (void)puts(
            "/FILES is not compatible with /COUNT."
        );
        return;
    }

    if (!command_decode_escapes(pattern)) {
        (void)puts(
            "Invalid escape sequence in GREP text."
        );
        return;
    }

    project_grep(
        state,
        pattern,
        path,
        (unsigned int)context_value,
        limit_value,
        case_sensitive,
        count_only,
        files_only,
        name_pattern,
        exclude_pattern,
        (unsigned int)depth_value
    );
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
