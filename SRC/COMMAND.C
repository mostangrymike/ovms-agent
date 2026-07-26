#include <stdio.h>
#include <string.h>

#include "command.h"
#include "command_internal.h"
#include "util.h"

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

    while (*position != '\0' &&
           *position != ' ' &&
           *position != '\t') {
        ++position;
    }

    if (*position != '\0') {
        *position = '\0';
        arguments = util_skip_space(position + 1);
    } else {
        arguments = position;
    }

    util_uppercase(command);

    entry = command_find(command);

    if (entry != NULL) {
        entry->handler(state, arguments);
        return;
    }

    if (strcmp(command, "LIST/ALL") == 0) {
        extern void project_list(agent_state *,
                                 const char *,
                                 int);
        project_list(state, arguments, 1);
        return;
    }

    (void)printf("Unknown command: %s\n", command);
    (void)puts("Enter HELP for available commands.");
}

int command_decode_escapes(char *text)
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

char *command_next_argument(char **cursor)
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
