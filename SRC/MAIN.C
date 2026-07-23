/*
 * OVMS Agent
 * Native agentic programming assistant for OpenVMS x86-64.
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define OVMS_AGENT_VERSION "0.1.0"
#define INPUT_SIZE 1024

static void trim_line(char *text)
{
    size_t length;

    if (text == NULL) {
        return;
    }

    length = strlen(text);

    while (length > 0 &&
           (text[length - 1] == '\n' ||
            text[length - 1] == '\r' ||
            isspace((unsigned char)text[length - 1]))) {
        text[--length] = '\0';
    }
}

static void uppercase(char *text)
{
    unsigned char *position;

    if (text == NULL) {
        return;
    }

    position = (unsigned char *)text;

    while (*position != '\0') {
        *position = (unsigned char)toupper(*position);
        ++position;
    }
}

static void show_banner(void)
{
    puts("OVMS Agent");
    puts("Native agentic programming assistant for OpenVMS");
    printf("Version %s\n\n", OVMS_AGENT_VERSION);
}

static void show_help(void)
{
    puts("Commands:");
    puts("  HELP       Display this help");
    puts("  VERSION    Display the agent version");
    puts("  ROOT       Display the permitted project root");
    puts("  STATUS     Display current agent status");
    puts("  QUIT       Exit OVMS Agent");
}

static void show_root(void)
{
    const char *root;

    root = getenv("OVMS_AGENT_ROOT");

    if (root == NULL || *root == '\0') {
        puts("OVMS_AGENT_ROOT is not defined.");
        puts("Define it before allowing project access:");
        puts("$ DEFINE/PROCESS OVMS_AGENT_ROOT device:[directory]");
        return;
    }

    printf("Project root: %s\n", root);
}

static void show_status(void)
{
    const char *root;
    const char *api_key;

    root = getenv("OVMS_AGENT_ROOT");
    api_key = getenv("OPENAI_API_KEY");

    printf("Version:       %s\n", OVMS_AGENT_VERSION);
    printf("Project root:  %s\n",
           root != NULL && *root != '\0' ? root : "not defined");
    printf("API key:       %s\n",
           api_key != NULL && *api_key != '\0' ? "defined" : "not defined");
    puts("Write access:  disabled");
    puts("DCL execution: disabled");
}

static int process_command(char *command)
{
    trim_line(command);
    uppercase(command);

    if (*command == '\0') {
        return 1;
    }

    if (strcmp(command, "HELP") == 0) {
        show_help();
    } else if (strcmp(command, "VERSION") == 0) {
        printf("OVMS Agent %s\n", OVMS_AGENT_VERSION);
    } else if (strcmp(command, "ROOT") == 0) {
        show_root();
    } else if (strcmp(command, "STATUS") == 0) {
        show_status();
    } else if (strcmp(command, "QUIT") == 0 ||
               strcmp(command, "EXIT") == 0) {
        return 0;
    } else {
        printf("Unknown command: %s\n", command);
        puts("Enter HELP for available commands.");
    }

    return 1;
}

int main(void)
{
    char input[INPUT_SIZE];

    show_banner();

    for (;;) {
        fputs("OVMS-AGENT> ", stdout);
        fflush(stdout);

        if (fgets(input, sizeof(input), stdin) == NULL) {
            putchar('\n');
            break;
        }

        if (!process_command(input)) {
            break;
        }
    }

    puts("OVMS Agent terminated.");
    return EXIT_SUCCESS;
}
