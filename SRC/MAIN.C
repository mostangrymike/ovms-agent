#include <stdio.h>
#include <stdlib.h>

#include "agent.h"
#include "command.h"

int main(void)
{
    agent_state state;
    char input[OVMS_AGENT_INPUT_SIZE];

    if (!agent_initialize(&state)) {
        (void)fputs("Unable to initialize OVMS Agent.\n", stderr);
        return EXIT_FAILURE;
    }

    while (agent_is_running(&state)) {
        command_prompt();

        if (fgets(input, sizeof(input), stdin) == NULL) {
            (void)putchar('\n');
            break;
        }

        command_execute(&state, input);
    }

    agent_shutdown(&state);
    return EXIT_SUCCESS;
}
