#include <stdio.h>
#include <string.h>

#include "agent.h"
#include "command_internal.h"
#include "util.h"

static const command_entry core_commands[] = {
    { "HELP", "Display command help", command_help },
    { "VERSION", "Display agent version", command_version },
    { "QUIT", "Exit OVMS Agent", command_quit },
    { "EXIT", "Exit OVMS Agent", command_quit }
};

void command_register_core(void)
{
    (void)command_registry_add(
        core_commands,
        sizeof(core_commands) / sizeof(core_commands[0])
    );
}

void command_help(agent_state *state, const char *arguments)
{
    const command_entry *entry;

    (void)state;
    (void)arguments;
    (void)puts("Commands:");

    size_t index;

    for (index = 0U;
         index < command_registry_count();
         ++index) {
        entry = command_registry_get(index);

        if (entry != NULL &&
            strcmp(entry->name, "EXIT") != 0) {
            (void)printf(
                "  %-10s %s\n",
                entry->name,
                entry->description
            );
        }
    }
}

void command_version(agent_state *state,
                     const char *arguments)
{
    (void)state;
    (void)arguments;
    (void)printf("OVMS Agent %s\n", OVMS_AGENT_VERSION);
}

void command_quit(agent_state *state,
                  const char *arguments)
{
    (void)arguments;
    agent_stop(state);
}
