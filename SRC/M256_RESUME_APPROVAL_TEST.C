#include <stdio.h>
#include <stdlib.h>

#include "openai_internal.h"

#define TEST_DATA "M256_SESSIONS.DAT"
#define TEST_CUR  "M256_SESSION.CUR"
#define TEST_TARGET "TEST/M256_CKPT_TARGET.TMP"
#define PLAN_FILE "OVMS_AGENT_PLAN.TXT"
#define PLAN_CHECK "OVMS_AGENT_PLAN.TXT.CHK"

extern int openai_plan_approved;
extern unsigned long openai_approved_hash;
extern int openai_approval_invalidated;

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

static void remove_all(const char *path)
{
    while (remove(path) == 0) {
    }
}

static int write_target(const char *text)
{
    FILE *file;

    file = fopen(TEST_TARGET, "w");
    if (file == NULL) return 0;
    if (fputs(text, file) == EOF) {
        (void)fclose(file);
        return 0;
    }
    return fclose(file) == 0;
}

static void cleanup(void)
{
    openai_test_session_paths(NULL, NULL);
    openai_plan_session_reset();
    openai_test_ckpt_clear();
    remove_all(TEST_DATA);
    remove_all(TEST_CUR);
    remove_all(TEST_TARGET);
    remove_all(PLAN_FILE);
    remove_all(PLAN_CHECK);
}

int main(void)
{
    char id[9];
    const char *plan_text;

    cleanup();
    openai_test_session_paths(TEST_DATA, TEST_CUR);

    if (!openai_session_new("m256 resume approval", id)) {
        (void)puts("M256 failed: unable to create session fixture.");
        cleanup();
        return EXIT_FAILURE;
    }

    /* Simulate approval that existed before the user resumed the session. */
    openai_plan_approved = 1;
    openai_approved_hash = 0x12345678UL;
    openai_approval_invalidated = 1;

    if (!openai_session_resume(id)) {
        (void)puts("M256 failed: session resume unexpectedly failed.");
        cleanup();
        return EXIT_FAILURE;
    }

    if (openai_plan_approved != 0 ||
        openai_approved_hash != 0UL ||
        openai_approval_invalidated != 0) {
        (void)puts("M256 failed: resumed session retained stale plan approval.");
        cleanup();
        return EXIT_FAILURE;
    }

    if (!write_target("alpha\n")) {
        (void)puts("M256 failed: unable to create checkpoint target.");
        cleanup();
        return EXIT_FAILURE;
    }

    plan_text =
        "Files to modify\n"
        "TEST/M256_CKPT_TARGET.TMP\n"
        "operation_count=1\n"
        "BEGIN_OPERATION\n"
        "type=replace_text\n"
        "path=TEST/M256_CKPT_TARGET.TMP\n"
        "old_text=alpha\n"
        "new_text=beta\n"
        "END_OPERATION\n";

    if (!openai_plan_save("m256 checkpoint goal", plan_text)) {
        (void)puts("M256 failed: unable to save checkpoint plan.");
        cleanup();
        return EXIT_FAILURE;
    }

    /* Same persistent current session owns this active plan and may bind it. */
    if (!openai_session_resume(id)) {
        (void)puts("M256 failed: unable to bind active plan checkpoint.");
        cleanup();
        return EXIT_FAILURE;
    }

    /* Change a fingerprinted file. Silent continuation must now be refused. */
    if (!write_target("alpha changed\n") ||
        openai_session_resume(id)) {
        (void)puts("M256 failed: stale planned file did not block resume.");
        cleanup();
        return EXIT_FAILURE;
    }

    cleanup();
    (void)puts("M256 session resume reapproval/checkpoint evidence passed.");
    return EXIT_SUCCESS;
}
