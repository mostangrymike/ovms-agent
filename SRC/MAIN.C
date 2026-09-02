#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "agent.h"
#include "command.h"
#include "command_internal.h"
#include "ANSI_TERM.H"

#include "command_input_m149.inc"
#include "command_input_m251.inc"
#include "command_input_m299.inc"

#define MAIN_FAILURE_STATUS 0x10000002U
#define MAIN_ONESHOT_OUTPUT 8192U

void m273_cmd_set_gitdiff(int (*callback)(void));
void m273_retry_set_gitdiff(int (*callback)(void));

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

static int main_equal_ci(const char *left, const char *right)
{
    unsigned char a;
    unsigned char b;

    if (left == NULL || right == NULL) {
        return 0;
    }

    while (*left != '\0' && *right != '\0') {
        a = (unsigned char)toupper((unsigned char)*left++);
        b = (unsigned char)toupper((unsigned char)*right++);

        if (a != b) {
            return 0;
        }
    }

    return *left == '\0' && *right == '\0';
}

static int main_starts_ci(const char *text, const char *prefix)
{
    unsigned char a;
    unsigned char b;

    if (text == NULL || prefix == NULL) {
        return 0;
    }

    while (*prefix != '\0') {
        if (*text == '\0') {
            return 0;
        }

        a = (unsigned char)toupper((unsigned char)*text++);
        b = (unsigned char)toupper((unsigned char)*prefix++);

        if (a != b) {
            return 0;
        }
    }

    return 1;
}

static void main_json_string(const char *text)
{
    const unsigned char *cursor;

    (void)putchar('"');

    cursor = (const unsigned char *)(text != NULL ? text : "");
    while (*cursor != (unsigned char)'\0') {
        unsigned char ch;

        ch = *cursor++;

        if (ch == (unsigned char)'"' || ch == (unsigned char)'\\') {
            (void)putchar('\\');
            (void)putchar((int)ch);
        } else if (ch == (unsigned char)'\n') {
            (void)fputs("\\n", stdout);
        } else if (ch == (unsigned char)'\r') {
            (void)fputs("\\r", stdout);
        } else if (ch == (unsigned char)'\t') {
            (void)fputs("\\t", stdout);
        } else if (ch >= 32U && ch < 127U) {
            (void)putchar((int)ch);
        } else {
            (void)putchar('?');
        }
    }

    (void)putchar('"');
}

static int main_one_shot(agent_state *state,
                         const char *spec,
                         const char *mode)
{
    const char *command;
    char output[MAIN_ONESHOT_OUTPUT];
    unsigned long status;
    int json_mode;
    int executed;

    json_mode = main_equal_ci(mode, "JSON");

    if (state == NULL || spec == NULL || *spec == '\0' ||
        !main_starts_ci(spec, "DCL ")) {
        if (json_mode) {
            (void)fputs(
                "{\"schema\":\"ovms-agent-v1\","
                "\"mode\":\"one-shot\","
                "\"status\":\"invalid\","
                "\"message\":\"expected DCL <command>\"}\n",
                stdout
            );
        } else {
            (void)puts("One-shot mode requires: DCL <command>");
        }
        return (int)MAIN_FAILURE_STATUS;
    }

    command = spec + 4;
    while (*command == ' ' || *command == '\t') {
        ++command;
    }

    executed = command_dcl_exec(
        state,
        command,
        output,
        sizeof(output),
        &status
    );

    if (json_mode) {
        (void)fputs(
            "{\"schema\":\"ovms-agent-v1\","
            "\"mode\":\"one-shot\","
            "\"operation\":\"DCL\","
            "\"executed\":",
            stdout
        );
        (void)fputs(executed ? "true" : "false", stdout);
        (void)fputs(",\"condition\":\"", stdout);
        (void)printf("%%X%08lX", status);
        (void)fputs("\",\"success\":", stdout);
        (void)fputs(",\"success\":", stdout);
        (void)fputs(
            executed && ((status & 1UL) != 0UL) ? "true" : "false",
            stdout
        );
        (void)fputs(",\"output\":", stdout);
        main_json_string(output);
        (void)fputs("}\n", stdout);
    } else {
        if (output[0] != '\0') {
            (void)fputs(output, stdout);
            if (strchr(output, '\n') == NULL) {
                (void)putchar('\n');
            }
        }
        (void)printf(
            "OpenVMS completion status: %%X%08lX (%s)\n",
            status,
            executed && ((status & 1UL) != 0UL) ?
                "success" : "failure"
        );
    }

    if (!executed) {
        return (int)MAIN_FAILURE_STATUS;
    }

    return (int)status;
}

int main(void)
{
    agent_state state;
    char input[OVMS_AGENT_INPUT_SIZE];
    char multiline_prefix[OVMS_AGENT_INPUT_SIZE];
    const char *one_shot;
    const char *result_mode;
    int read_status;
    int final_status;

    ansi_term_init();
    m273_cmd_set_gitdiff(ansi_term_git_diff);
    m273_retry_set_gitdiff(ansi_term_git_diff);

    one_shot = getenv("OVMS_AGENT_ONESHOT");
    result_mode = getenv("OVMS_AGENT_RESULT_MODE");

    if (!agent_initialize(&state)) {
        (void)fputs("Unable to initialize OVMS Agent.\n", stderr);
        return EXIT_FAILURE;
    }

    if (one_shot != NULL && *one_shot != '\0') {
        final_status = main_one_shot(&state, one_shot, result_mode);
        agent_shutdown(&state);
        return final_status;
    }

    while (agent_is_running(&state)) {
        ansi_term_status_clear();
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

        if (main_multiline_prefix(
                input,
                multiline_prefix,
                sizeof(multiline_prefix))) {
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

    ansi_term_status_clear();
    agent_shutdown(&state);
    return EXIT_SUCCESS;
}
