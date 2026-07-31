#include <stdio.h>
#include <stdlib.h>

#include "agent.h"
#include "command.h"

#include "command_input_m149.inc"

int main(void)
{
    agent_state state;
    char input[OVMS_AGENT_INPUT_SIZE];
    int read_status;

    if (!agent_initialize(&state)) {
        (void)fputs("Unable to initialize OVMS Agent.\n", stderr);
        return EXIT_FAILURE;
    }

    while (agent_is_running(&state)) {
        command_prompt();

        read_status = command_read_stream(
            stdin,
            input,
            sizeof(input));

        if (read_status == 0) {
            (void)putchar('\n');
            break;
        }

        if (read_status < 0) {
            (void)puts("Command line is too long.");
            continue;
        }

        command_execute(&state, input);
    }

    agent_shutdown(&state);
    return EXIT_SUCCESS;
}
