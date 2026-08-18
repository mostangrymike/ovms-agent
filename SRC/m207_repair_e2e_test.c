#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "agent.h"
#include "openai_internal.h"

#define TARGET_A "TEST/M207_TARGET_A.TMP"
#define TARGET_B "TEST/M207_TARGET_B.TMP"

static int build_call_count;
static int post_build_succeeds;

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

static char *m207_build_hook(int *build_status)
{
    ++build_call_count;

    if (build_status == NULL) {
        return NULL;
    }

    if (build_call_count == 1) {
        *build_status = 2;
        return copy_text(
            "OpenVMS build status: 2 (failure)\n\n"
            "%CC-E-UNDECLARED, identifier M207_BROKEN is undefined.\n"
        );
    }

    if (post_build_succeeds) {
        *build_status = 1;
        return copy_text(
            "OpenVMS build status: 1 (success)\n\n"
            "Controlled M207 rebuild passed.\n"
        );
    }

    *build_status = 2;
    return copy_text(
        "OpenVMS build status: 2 (failure)\n\n"
        "Controlled M207 rebuild still fails.\n"
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

/*
 * Link-only stubs required by OPENAI_PLAN.OBJ. The M207 test never uses
 * interactive command-stream input.
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

static const char repair_plan[] =
    "Files to modify\n"
    "TEST/M207_TARGET_A.TMP\n"
    "TEST/M207_TARGET_B.TMP\n"
    "\n"
    "operation_count=2\n"
    "BEGIN_OPERATION\n"
    "type=replace_block\n"
    "path=TEST/M207_TARGET_A.TMP\n"
    "BEGIN_OLD_TEXT\n"
    "ALPHA\n"
    "END_OLD_TEXT\n"
    "BEGIN_NEW_TEXT\n"
    "ALPHA_FIXED\n"
    "END_NEW_TEXT\n"
    "END_OPERATION\n"
    "BEGIN_OPERATION\n"
    "type=replace_block\n"
    "path=TEST/M207_TARGET_B.TMP\n"
    "BEGIN_OLD_TEXT\n"
    "BETA\n"
    "END_OLD_TEXT\n"
    "BEGIN_NEW_TEXT\n"
    "BETA_FIXED\n"
    "END_NEW_TEXT\n"
    "END_OPERATION\n";

static int prepare_targets(void)
{
    remove_all_versions(TARGET_A);
    remove_all_versions(TARGET_B);

    return write_text(TARGET_A, "ALPHA\n") &&
           write_text(TARGET_B, "BETA\n");
}

static int verify_contains(const char *path, const char *expected)
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
    remove_all_versions("OVMS_AGENT_FAILED_BUILD.TXT");
    remove_all_versions("OVMS_AGENT_FAILED_OPERATIONS.TXT");
}

static int run_scenario(int rebuild_succeeds)
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
    post_build_succeeds = rebuild_succeeds;

    openai_test_set_build_hook(m207_build_hook);
    openai_test_set_repair_plan(repair_plan, 1);

    openai_agent_repair(
        &state,
        "Repair the deterministic M207 failed build"
    );

    if (build_call_count != 2) {
        return 0;
    }

    if (rebuild_succeeds) {
        return verify_contains(TARGET_A, "ALPHA_FIXED") &&
               verify_contains(TARGET_B, "BETA_FIXED");
    }

    return verify_contains(TARGET_A, "ALPHA\n") &&
           verify_contains(TARGET_B, "BETA\n") &&
           !verify_contains(TARGET_A, "ALPHA_FIXED") &&
           !verify_contains(TARGET_B, "BETA_FIXED");
}

int main(void)
{
    int ok;

    cleanup();

    ok = run_scenario(1);

    if (!ok) {
        (void)puts("M207 failed: commit-on-success scenario failed.");
        cleanup();
        return EXIT_FAILURE;
    }

    cleanup();

    ok = run_scenario(0);

    if (!ok) {
        (void)puts("M207 failed: rollback-on-failure scenario failed.");
        cleanup();
        return EXIT_FAILURE;
    }

    cleanup();
    (void)puts("End-to-end deterministic AGENT/REPAIR test passed.");
    return EXIT_SUCCESS;
}
