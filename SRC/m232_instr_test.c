#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "llm_internal.h"

int llm_instr_reload(const agent_state *state);
int llm_instr_status_text(const agent_state *state,
                          char *output, size_t output_size);
int llm_instr_show_text(const agent_state *state,
                        char *output, size_t output_size);
int llm_instr_compose(const agent_state *state,
                      const char *goal,
                      char *output, size_t output_size);
void llm_test_instr_path(const char *path);

#define TEST_FILE "M232_INSTRUCTIONS.TXT"

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

static void remove_all(const char *path)
{
    while (remove(path) == 0) {
    }
}

static void cleanup(void)
{
    llm_test_instr_path(NULL);
    remove_all(TEST_FILE);
}

int main(void)
{
    agent_state state;
    FILE *file;
    char output[16384];

    cleanup();
    (void)memset(&state, 0, sizeof(state));
    state.project_root = ".";

    llm_test_instr_path(TEST_FILE);

    if (!llm_instr_reload(&state) ||
        !llm_instr_status_text(
            &state, output, sizeof(output)) ||
        strstr(output, "Status:    not found") == NULL) {
        (void)puts("M232 failed: missing instruction state.");
        cleanup();
        return EXIT_FAILURE;
    }

    file = fopen(TEST_FILE, "w");

    if (file == NULL) {
        (void)puts("M232 failed: create instruction file.");
        cleanup();
        return EXIT_FAILURE;
    }

    (void)fputs(
        "Always preserve VMS file versions until verified.\n"
        "Keep external identifiers within 31 characters.\n",
        file
    );
    (void)fclose(file);

    if (!llm_instr_reload(&state) ||
        !llm_instr_status_text(
            &state, output, sizeof(output)) ||
        strstr(output, "Status:    loaded") == NULL ||
        strstr(output, "Truncated: no") == NULL) {
        (void)puts("M232 failed: instruction reload.");
        cleanup();
        return EXIT_FAILURE;
    }

    if (!llm_instr_show_text(
            &state, output, sizeof(output)) ||
        strstr(
            output,
            "Always preserve VMS file versions until verified."
        ) == NULL ||
        strstr(
            output,
            "Keep external identifiers within 31 characters."
        ) == NULL) {
        (void)puts("M232 failed: instruction display.");
        cleanup();
        return EXIT_FAILURE;
    }

    if (!llm_instr_compose(
            &state,
            "Fix the parser.",
            output,
            sizeof(output)) ||
        strstr(output, "PROJECT INSTRUCTIONS") == NULL ||
        strstr(output, "CURRENT REQUEST") == NULL ||
        strstr(output, "Fix the parser.") == NULL ||
        strstr(
            output,
            "Keep external identifiers within 31 characters."
        ) == NULL) {
        (void)puts("M232 failed: instruction composition.");
        cleanup();
        return EXIT_FAILURE;
    }

    if (!openai_parity_text(output, sizeof(output)) ||
        strstr(output, "Project instructions:  available") == NULL ||
        strstr(output, "Instruction reload:    available") == NULL) {
        (void)puts("M232 failed: parity status.");
        cleanup();
        return EXIT_FAILURE;
    }

    cleanup();
    (void)puts("Project instruction context bundle test passed.");
    return EXIT_SUCCESS;
}
