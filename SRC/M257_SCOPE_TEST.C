#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "llm_internal.h"

#define ROOT_FILE "M257_ROOT_INSTR.TXT"
#define SCOPE_FILE "TEST/OVMS_AGENT_INSTRUCTIONS.TXT"

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
    while (remove(path) == 0) { }
}

static int exists(const char *path)
{
    FILE *file;
    file = fopen(path, "r");
    if (file == NULL) return 0;
    (void)fclose(file);
    return 1;
}

static int write_text(const char *path, const char *text)
{
    FILE *file;
    file = fopen(path, "w");
    if (file == NULL) return 0;
    if (fputs(text, file) == EOF) {
        (void)fclose(file);
        return 0;
    }
    return fclose(file) == 0;
}

static void cleanup(int scoped_created)
{
    openai_test_instr_path(NULL);
    remove_all(ROOT_FILE);
    if (scoped_created) remove_all(SCOPE_FILE);
}

int main(void)
{
    agent_state state;
    char output[16384];
    char status[4096];
    char *root_pos;
    char *scope_pos;
    int scoped_created;

    scoped_created = 0;
    remove_all(ROOT_FILE);

    if (exists(SCOPE_FILE)) {
        (void)puts("M257 failed: scoped instruction fixture already exists.");
        return EXIT_FAILURE;
    }

    if (!write_text(ROOT_FILE,
                    "scope_rule=ROOT\n"
                    "root_only=present\n") ||
        !write_text(SCOPE_FILE,
                    "scope_rule=TEST\n"
                    "scoped_only=present\n")) {
        scoped_created = exists(SCOPE_FILE);
        (void)puts("M257 failed: unable to create instruction fixtures.");
        cleanup(scoped_created);
        return EXIT_FAILURE;
    }
    scoped_created = 1;

    (void)memset(&state, 0, sizeof(state));
    state.project_root = ".";
    openai_test_instr_path(ROOT_FILE);

    if (!openai_instr_reload(&state) ||
        !openai_instr_compose(
            &state,
            "Plan a safe change to TEST/M257_TARGET.C",
            output,
            sizeof(output))) {
        (void)puts("M257 failed: scoped instruction composition.");
        cleanup(scoped_created);
        return EXIT_FAILURE;
    }

    root_pos = strstr(output, "root_only=present");
    scope_pos = strstr(output, "scoped_only=present");

    if (root_pos == NULL || scope_pos == NULL ||
        root_pos >= scope_pos ||
        strstr(output, "INSTRUCTION PRECEDENCE") == NULL ||
        strstr(output, "later, deeper scopes override") == NULL ||
        strstr(output, "DIRECTORY-SCOPED PROJECT INSTRUCTIONS [TEST]") == NULL) {
        (void)puts("M257 failed: root/scoped precedence context.");
        cleanup(scoped_created);
        return EXIT_FAILURE;
    }

    if (!openai_instr_status_text(
            &state, status, sizeof(status)) ||
        strstr(status, "Scoped active files: 1") == NULL ||
        strstr(status, SCOPE_FILE) == NULL ||
        strstr(status, "Scoped truncated:   no") == NULL) {
        (void)puts("M257 failed: active scoped-file reporting.");
        cleanup(scoped_created);
        return EXIT_FAILURE;
    }

    if (!openai_instr_compose(
            &state,
            "Describe the current project",
            output,
            sizeof(output)) ||
        strstr(output, "root_only=present") == NULL ||
        strstr(output, "scoped_only=present") != NULL ||
        !openai_instr_status_text(
            &state, status, sizeof(status)) ||
        strstr(status, "Scoped active files: 0") == NULL) {
        (void)puts("M257 failed: root-only fallback for unscoped goal.");
        cleanup(scoped_created);
        return EXIT_FAILURE;
    }

    cleanup(scoped_created);
    (void)puts("M257 directory-scoped instruction precedence test passed.");
    return EXIT_SUCCESS;
}
