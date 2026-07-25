#include <stdio.h>
#include <string.h>

#include "agent.h"
#include "command_internal.h"
#include "util.h"

void command_help(agent_state *state, const char *arguments)
{
    const command_entry *entry;

    (void)state;
    (void)arguments;
    (void)puts("Commands:");

    for (entry = command_table_get();
         entry->name != NULL;
         ++entry) {
        if (strcmp(entry->name, "EXIT") != 0) {
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
