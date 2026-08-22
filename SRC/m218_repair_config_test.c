#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "llm_internal.h"

/*
 * Link-only stubs required by LLM_PLAN.OBJ.
 * This deterministic configuration test never uses interactive input.
 */
int command_line_complete(const char *input,
                          size_t input_size,
                          int reached_eof)
{
    (void)input;
    (void)input_size;
    (void)reached_eof;
    return 0;
}

int command_read_stream(FILE *stream,
                        char *input,
                        size_t input_size)
{
    (void)stream;
    (void)input;
    (void)input_size;
    return 0;
}

static int contains_all(const char *text,
                        const char *first,
                        const char *second,
                        const char *third)
{
    return
        strstr(text, first) != NULL &&
        strstr(text, second) != NULL &&
        strstr(text, third) != NULL;
}

int main(void)
{
    char configuration[1024];

    if (!llm_test_repair_config_text(
            NULL,
            configuration,
            sizeof(configuration)) ||
        !contains_all(
            configuration,
            "History window:       5",
            "Maximum window:       20",
            "Source:               default")) {
        (void)puts("M218 failed: missing value did not report default.");
        return EXIT_FAILURE;
    }

    if (!llm_test_repair_config_text(
            "12",
            configuration,
            sizeof(configuration)) ||
        !contains_all(
            configuration,
            "History window:       12",
            "Default window:       5",
            "Source:               environment")) {
        (void)puts("M218 failed: valid environment value was not reported.");
        return EXIT_FAILURE;
    }

    if (!llm_test_repair_config_text(
            "abc",
            configuration,
            sizeof(configuration)) ||
        strstr(configuration, "History window:       5") == NULL ||
        strstr(configuration, "Source:               default") == NULL) {
        (void)puts("M218 failed: invalid text did not fall back.");
        return EXIT_FAILURE;
    }

    if (!llm_test_repair_config_text(
            "21",
            configuration,
            sizeof(configuration)) ||
        strstr(configuration, "History window:       5") == NULL ||
        strstr(configuration, "Source:               default") == NULL) {
        (void)puts("M218 failed: out-of-range value did not fall back.");
        return EXIT_FAILURE;
    }

    if (strstr(
            configuration,
            "Environment variable: OVMS_AGENT_REPAIR_HISTORY_RUNS") == NULL) {
        (void)puts("M218 failed: environment variable name missing.");
        return EXIT_FAILURE;
    }

    (void)puts("Repair configuration display test passed.");
    return EXIT_SUCCESS;
}
