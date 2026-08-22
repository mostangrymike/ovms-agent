#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "llm_internal.h"
#include "LLM_PROJECT_MAP.H"

#define M233_COB "SRC/000_M233_SAMPLE.COB"
#define M233_CBL "SRC/000_M233_SAMPLE.CBL"
#define M233_CPY "SRC/000_M233_SAMPLE.CPY"
#define M233_TST "SRC/000_M233_PROJECT_TEST_SAMPLE.C"

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

static void m233_cleanup(void)
{
    while (remove(M233_COB) == 0) {
    }
    while (remove(M233_CBL) == 0) {
    }
    while (remove(M233_CPY) == 0) {
    }
    while (remove(M233_TST) == 0) {
    }
}

static int m233_write_sample(const char *path,
                             const char *text)
{
    FILE *sample;

    sample = fopen(path, "w");
    if (sample == NULL) {
        return 0;
    }

    (void)fputs(text, sample);
    return fclose(sample) == 0;
}

int main(void)
{
    agent_state state;
    char output[24576];

    while (remove("M233_PROJECT_MAP_DIAG.TXT") == 0) {
    }
    m233_cleanup();

    if (!m233_write_sample(
            M233_COB, "       IDENTIFICATION DIVISION.\n") ||
        !m233_write_sample(
            M233_CBL, "       IDENTIFICATION DIVISION.\n") ||
        !m233_write_sample(
            M233_CPY, "       01  M233-SAMPLE PIC X.\n") ||
        !m233_write_sample(
            M233_TST, "int m233_test_sample(void) { return 1; }\n")) {
        (void)puts("M233 failed: unable to create map samples.");
        m233_cleanup();
        return EXIT_FAILURE;
    }

    (void)memset(&state, 0, sizeof(state));
    state.project_root = ".";

    if (!llm_project_refresh(&state)) {
        (void)puts("M233 failed: project refresh.");
        m233_cleanup();
        return EXIT_FAILURE;
    }

    if (!llm_project_map_text(
            &state, output, sizeof(output)) ||
        strstr(output, "OVMS Agent project map") == NULL ||
        strstr(output, "Sources:") == NULL ||
        strstr(output, "Headers:") == NULL ||
        strstr(output, "Tests:") == NULL ||
        strstr(output, "Build:") == NULL ||
        (strstr(output, "M233_SAMPLE.CPY") == NULL &&
         strstr(output, "m233_sample.cpy") == NULL)) {
        (void)puts("M233 failed: project map.");
        m233_cleanup();
        return EXIT_FAILURE;
    }

    if (!llm_project_src_text(
            &state, output, sizeof(output)) ||
        (strstr(output, "LLM_") == NULL &&
         strstr(output, "llm_") == NULL) ||
        (strstr(output, "M233_SAMPLE.COB") == NULL &&
         strstr(output, "m233_sample.cob") == NULL) ||
        (strstr(output, "M233_SAMPLE.CBL") == NULL &&
         strstr(output, "m233_sample.cbl") == NULL) ||
        strstr(output, "M233_SAMPLE.CPY") != NULL ||
        strstr(output, "m233_sample.cpy") != NULL ||
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
        m233_cleanup();
        return EXIT_FAILURE;
    }

    if (!llm_project_tests_text(
            &state, output, sizeof(output)) ||
        (strstr(output, "M233_PROJECT_TEST_SAMPLE.C") == NULL &&
         strstr(output, "m233_project_test_sample.c") == NULL)) {
        (void)puts("M233 failed: test classification.");
        m233_cleanup();
        return EXIT_FAILURE;
    }

    if (!llm_project_build_text(
            &state, output, sizeof(output)) ||
        (strstr(output, "BUILD.COM") == NULL &&
         strstr(output, "build.com") == NULL)) {
        (void)puts("M233 failed: build classification.");
        m233_cleanup();
        return EXIT_FAILURE;
    }

    if (!llm_project_compose(
            &state, "Inspect parser.", output, sizeof(output)) ||
        strstr(output, "REPOSITORY MAP") == NULL ||
        strstr(output, "MODEL TASK CONTEXT") == NULL ||
        strstr(output, "Inspect parser.") == NULL) {
        (void)puts("M233 failed: repository context composition.");
        m233_cleanup();
        return EXIT_FAILURE;
    }

    if (!llm_parity_text(output, sizeof(output)) ||
        strstr(output, "Repository map:       available") == NULL ||
        strstr(output, "Context preloading:   available") == NULL) {
        (void)puts("M233 failed: parity status.");
        m233_cleanup();
        return EXIT_FAILURE;
    }

    m233_cleanup();
    (void)puts("Autonomous repository map bundle test passed.");
    return EXIT_SUCCESS;
}
