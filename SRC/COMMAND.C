#include <stdio.h>
#include <string.h>

#include "command.h"
#include "project.h"
#include "util.h"

typedef void (*command_handler)(agent_state *state);

typedef struct command_entry {
    const char *name;
    const char *description;
    command_handler handler;
} command_entry;

static void command_help(agent_state *state);
static void command_version(agent_state *state);
static void command_root(agent_state *state);
static void command_status(agent_state *state);
static void command_list(agent_state *state);
static void command_quit(agent_state *state);

static const command_entry command_table[] = {
    { "HELP",    "Display command help", command_help },
    { "VERSION", "Display agent version", command_version },
    { "ROOT",    "Display the project root", command_root },
    { "STATUS",  "Display agent status", command_status },
    { "LIST",    "List the project root", command_list },
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

    if (state == NULL || input == NULL) {
        return;
    }

    util_trim(input);
    command = util_skip_space(input);
    util_uppercase(command);

    if (command == NULL || *command == '\0') {
        return;
    }

    for (entry = command_table; entry->name != NULL; ++entry) {
        if (strcmp(command, entry->name) == 0) {
            entry->handler(state);
            return;
        }
    }

    (void)printf("Unknown command: %s\n", command);
    (void)puts("Enter HELP for available commands.");
}

static void command_help(agent_state *state)
{
    const command_entry *entry;

    (void)state;
    (void)puts("Commands:");

    for (entry = command_table; entry->name != NULL; ++entry) {
        if (strcmp(entry->name, "EXIT") != 0) {
            (void)printf("  %-10s %s\n",
                         entry->name,
                         entry->description);
        }
    }
}

static void command_version(agent_state *state)
{
    (void)state;
    (void)printf("OVMS Agent %s\n", OVMS_AGENT_VERSION);
}

static void command_root(agent_state *state)
{
    project_show_root(state);
}

static void command_status(agent_state *state)
{
    project_show_status(state);
}

static void command_list(agent_state *state)
{
    project_list(state);
}

static void command_quit(agent_state *state)
{
    agent_stop(state);
}
