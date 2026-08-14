#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "agent.h"
#include "command.h"

#include "command_input_m149.inc"
#include "command_input_m251.inc"

static const char *main_multiline_prefix(char *input)
{
    size_t length;

    if (input == NULL) {
        return NULL;
    }

    length = strlen(input);

    while (length > 0U &&
           (input[length - 1U] == '\n' ||
            input[length - 1U] == '\r')) {
        input[--length] = '\0';
    }

    if (strcmp(input, "AGENT/MULTILINE") == 0) {
        return "AGENT";
    }

    if (strcmp(input, "AGENT/CREATE/MULTILINE") == 0) {
        return "AGENT/CREATE";
    }

    if (strcmp(input, "AGENT/REPAIR/MULTILINE") == 0) {
        return "AGENT/REPAIR";
    }

    return NULL;
}

static int main_read_multiline(
    char *input,
    size_t input_size,
    const char *prefix)
{
    size_t prefix_length;
    int status;

    if (input == NULL ||
        prefix == NULL ||
        input_size < 2U) {
        return 0;
    }

    prefix_length = strlen(prefix);

    if (prefix_length + 2U > input_size) {
        return -1;
    }

    (void)strcpy(input, prefix);
    input[prefix_length] = ' ';
    input[prefix_length + 1U] = '\0';

    (void)puts(
        "Enter multiline prompt; finish with .END on a line by itself."
    );

    status = command_read_multiline(
        stdin,
        input + prefix_length + 1U,
        input_size - prefix_length - 1U
    );

    if (status <= 0) {
        input[0] = '\0';
        return status;
    }

    if (input[prefix_length + 1U] == '\0') {
        input[0] = '\0';
        return -3;
    }

    return 1;
}

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
        const char *multiline_prefix;

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

        multiline_prefix = main_multiline_prefix(input);

        if (multiline_prefix != NULL) {
            read_status = main_read_multiline(
                input,
                sizeof(input),
                multiline_prefix
            );

            if (read_status == -1) {
                (void)puts("Multiline prompt is too long.");
                continue;
            }

            if (read_status == -2) {
                (void)puts(
                    "Multiline prompt ended before the .END terminator."
                );
                break;
            }

            if (read_status == -3) {
                (void)puts("Multiline prompt is empty.");
                continue;
            }

            if (read_status <= 0) {
                (void)puts("Unable to read multiline prompt.");
                continue;
            }
        }

        command_execute(&state, input);
    }

    agent_shutdown(&state);
    return EXIT_SUCCESS;
}
