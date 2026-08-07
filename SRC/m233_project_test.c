#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "openai_internal.h"

int command_line_complete(const char *input,
                          size_t input_size,
                          int reached_eof)
{
    (void)input; (void)input_size; (void)reached_eof; return 0;
}

int command_read_stream(FILE *stream,
                        char *input,
                        size_t input_size)
{
    (void)stream; (void)input; (void)input_size; return 0;
}

int main(void)
{
    agent_state state;
    char output[24576];

    while (remove("M233_PROJECT_MAP_DIAG.TXT") == 0) {
    }

    (void)memset(&state, 0, sizeof(state));
    state.project_root = ".";

    if (!openai_project_refresh(&state)) {
        (void)puts("M233 failed: project refresh.");
        return EXIT_FAILURE;
    }

    if (!openai_project_map_text(
            &state, output, sizeof(output)) ||
        strstr(output, "OVMS Agent project map") == NULL ||
        strstr(output, "Sources:") == NULL ||
        strstr(output, "Tests:") == NULL ||
        strstr(output, "Build:") == NULL) {
        (void)puts("M233 failed: project map.");
        return EXIT_FAILURE;
    }

    if (!openai_project_src_text(
            &state, output, sizeof(output)) ||
        (strstr(output, "OPENAI_") == NULL &&
         strstr(output, "openai_") == NULL) ||
        strstr(output, ".C;") != NULL ||
        strstr(output, ".c;") != NULL) {
        FILE *diag;

        diag = fopen("M233_PROJECT_MAP_DIAG.TXT", "w");
        if (diag != NULL) {
            (void)fputs(output, diag);
            (void)fclose(diag);
        }

        (void)puts("M233 failed: source classification.");
        (void)puts(
            "Repository map saved to M233_PROJECT_MAP_DIAG.TXT."
        );
        return EXIT_FAILURE;
    }

    if (!openai_project_tests_text(
            &state, output, sizeof(output)) ||
        (strstr(output, "M233_PROJECT_TEST.C") == NULL &&
         strstr(output, "m233_project_test.c") == NULL)) {
        (void)puts("M233 failed: test classification.");
        return EXIT_FAILURE;
    }

    if (!openai_project_build_text(
            &state, output, sizeof(output)) ||
        (strstr(output, "BUILD.COM") == NULL &&
         strstr(output, "build.com") == NULL)) {
        (void)puts("M233 failed: build classification.");
        return EXIT_FAILURE;
    }

    if (!openai_project_compose(
            &state, "Inspect parser.", output, sizeof(output)) ||
        strstr(output, "REPOSITORY MAP") == NULL ||
        strstr(output, "MODEL TASK CONTEXT") == NULL ||
        strstr(output, "Inspect parser.") == NULL) {
        (void)puts("M233 failed: repository context composition.");
        return EXIT_FAILURE;
    }

    if (!openai_parity_text(output, sizeof(output)) ||
        strstr(output, "Repository map:       available") == NULL ||
        strstr(output, "Context preloading:   available") == NULL) {
        (void)puts("M233 failed: parity status.");
        return EXIT_FAILURE;
    }

    (void)puts("Autonomous repository map bundle test passed.");
    return EXIT_SUCCESS;
}
