#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "agent.h"
#include "openai_internal.h"

#define TARGET_A "TEST/M208_TARGET_A.TMP"
#define TARGET_B "TEST/M208_TARGET_B.TMP"

static int build_call_count;
static int second_rebuild_succeeds;

static char *copy_text(const char *text)
{
    char *copy;
    size_t length;

    length = strlen(text);
    copy = (char *)malloc(length + 1U);

    if (copy != NULL) {
        (void)memcpy(copy, text, length + 1U);
    }

    return copy;
}

static char *m208_build_hook(int *build_status)
{
    ++build_call_count;

    if (build_status == NULL) {
        return NULL;
    }

    if (build_call_count == 1) {
        *build_status = 2;
        return copy_text(
            "OpenVMS build status: 2 (failure)\n\n"
            "%CC-E-UNDECLARED, identifier M208_INITIAL is undefined.\n"
        );
    }

    if (build_call_count == 2) {
        *build_status = 2;
        return copy_text(
            "OpenVMS build status: 2 (failure)\n\n"
            "%CC-E-UNDECLARED, identifier M208_ATTEMPT1 is undefined.\n"
        );
    }

    if (second_rebuild_succeeds) {
        *build_status = 1;
        return copy_text(
            "OpenVMS build status: 1 (success)\n\n"
            "Controlled M208 second repair rebuild passed.\n"
        );
    }

    *build_status = 2;
    return copy_text(
        "OpenVMS build status: 2 (failure)\n\n"
        "%CC-E-UNDECLARED, identifier M208_ATTEMPT2 is undefined.\n"
    );
}

static void remove_all_versions(const char *path)
{
    while (remove(path) == 0) {
    }
}

static int write_text(const char *path, const char *text)
{
    FILE *file;

    file = fopen(path, "w");
    if (file == NULL) {
        return 0;
    }

    if (fputs(text, file) == EOF) {
        (void)fclose(file);
        return 0;
    }

    return fclose(file) == 0;
}

static char *read_text(const char *path)
{
    FILE *file;
    long length;
    char *text;
    size_t got;

    file = fopen(path, "rb");
    if (file == NULL) {
        return NULL;
    }

    if (fseek(file, 0L, SEEK_END) != 0) {
        (void)fclose(file);
        return NULL;
    }

    length = ftell(file);
    if (length < 0L || fseek(file, 0L, SEEK_SET) != 0) {
        (void)fclose(file);
        return NULL;
    }

    text = (char *)malloc((size_t)length + 1U);
    if (text == NULL) {
        (void)fclose(file);
        return NULL;
    }

    got = fread(text, 1U, (size_t)length, file);
    text[got] = '\0';
    (void)fclose(file);
    return text;
}

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

static const char repair_plan1[] =
    "Files to modify\n"
    "TEST/M208_TARGET_A.TMP\n"
    "TEST/M208_TARGET_B.TMP\n"
    "\n"
    "operation_count=2\n"
    "BEGIN_OPERATION\n"
    "type=replace_text\n"
    "path=TEST/M208_TARGET_A.TMP\n"
    "old_text=ALPHA\n"
    "new_text=ALPHA_ONE\n"
    "END_OPERATION\n"
    "BEGIN_OPERATION\n"
    "type=replace_text\n"
    "path=TEST/M208_TARGET_B.TMP\n"
    "old_text=BETA\n"
    "new_text=BETA_ONE\n"
    "END_OPERATION\n";

static const char repair_plan2[] =
    "Files to modify\n"
    "TEST/M208_TARGET_A.TMP\n"
    "TEST/M208_TARGET_B.TMP\n"
    "\n"
    "operation_count=2\n"
    "BEGIN_OPERATION\n"
    "type=replace_text\n"
    "path=TEST/M208_TARGET_A.TMP\n"
    "old_text=ALPHA\n"
    "new_text=ALPHA_TWO\n"
    "END_OPERATION\n"
    "BEGIN_OPERATION\n"
    "type=replace_text\n"
    "path=TEST/M208_TARGET_B.TMP\n"
    "old_text=BETA\n"
    "new_text=BETA_TWO\n"
    "END_OPERATION\n";

static int prepare_targets(void)
{
    remove_all_versions(TARGET_A);
    remove_all_versions(TARGET_B);

    return write_text(TARGET_A, "ALPHA\n") &&
           write_text(TARGET_B, "BETA\n");
}

static int contains_text(const char *path, const char *expected)
{
    char *text;
    int result;

    text = read_text(path);
    if (text == NULL) {
        return 0;
    }

    result = strstr(text, expected) != NULL;
    free(text);
    return result;
}

static void cleanup(void)
{
    openai_test_set_repair_plan(NULL, 0);
    openai_test_set_build_hook(NULL);
    openai_plan_approval_clear();
    remove_all_versions(TARGET_A);
    remove_all_versions(TARGET_B);
    remove_all_versions("OVMS_AGENT_PLAN.TXT");
    remove_all_versions("OVMS_AGENT_PLAN.TXT.CHK");
    remove_all_versions("OVMS_AGENT_FAILED_BUILD.TXT");
    remove_all_versions("OVMS_AGENT_FAILED_OPERATIONS.TXT");
}

static int run_scenario(int second_succeeds)
{
    agent_state state;

    if (!prepare_targets()) {
        return 0;
    }

    (void)memset(&state, 0, sizeof(state));
    state.running = 1;
    state.project_root = ".";
    state.write_enabled = 1;
    state.dcl_enabled = 1;

    build_call_count = 0;
    second_rebuild_succeeds = second_succeeds;

    openai_test_set_build_hook(m208_build_hook);
    openai_test_set_repair_plans(
        repair_plan1,
        repair_plan2,
        1
    );

    openai_agent_repair(
        &state,
        "Repair the deterministic M208 failed build"
    );

    if (build_call_count != 3) {
        return 0;
    }

    if (second_succeeds) {
        return contains_text(TARGET_A, "ALPHA_TWO") &&
               contains_text(TARGET_B, "BETA_TWO") &&
               !contains_text(TARGET_A, "ALPHA_ONE") &&
               !contains_text(TARGET_B, "BETA_ONE");
    }

    return contains_text(TARGET_A, "ALPHA\n") &&
           contains_text(TARGET_B, "BETA\n") &&
           !contains_text(TARGET_A, "ALPHA_ONE") &&
           !contains_text(TARGET_B, "BETA_ONE") &&
           !contains_text(TARGET_A, "ALPHA_TWO") &&
           !contains_text(TARGET_B, "BETA_TWO");
}

int main(void)
{
    cleanup();

    if (!run_scenario(1)) {
        (void)puts(
            "M208 failed: second-attempt success scenario failed."
        );
        cleanup();
        return EXIT_FAILURE;
    }

    cleanup();

    if (!run_scenario(0)) {
        (void)puts(
            "M208 failed: two-attempt rollback scenario failed."
        );
        cleanup();
        return EXIT_FAILURE;
    }

    cleanup();
    (void)puts("Bounded two-attempt AGENT/REPAIR test passed.");
    return EXIT_SUCCESS;
}
