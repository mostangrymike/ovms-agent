#include <stdio.h>
#include <stdlib.h>

#include "openai_internal.h"

#define TEST_DATA "M256_SESSIONS.DAT"
#define TEST_CUR  "M256_SESSION.CUR"

extern int openai_plan_approved;
extern unsigned long openai_approved_hash;
extern int openai_approval_invalidated;
void openai_test_ckpt_clear(void);

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

static void cleanup(void)
{
    openai_test_session_paths(NULL, NULL);
    openai_plan_session_reset();
    openai_test_ckpt_clear();
    remove_all(TEST_DATA);
    remove_all(TEST_CUR);
    remove_all("M256_PLAN_TARGET.TMP");
    remove_all("OVMS_AGENT_PLAN.TXT");
    remove_all("OVMS_AGENT_PLAN.TXT.CHK");
}

static int write_target(const char *text)
{
    FILE *file;

    file = fopen("M256_PLAN_TARGET.TMP", "w");
    if (file == NULL) return 0;
    if (fputs(text, file) == EOF) {
        (void)fclose(file);
        return 0;
    }
    return fclose(file) == 0;
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

    if (!write_target("before\n")) {
        (void)puts("M256 failed: unable to write plan target fixture.");
        cleanup();
        return EXIT_FAILURE;
    }

    plan_text =
        "Files to modify\n"
        "M256_PLAN_TARGET.TMP\n"
        "operation_count=1\n"
        "BEGIN_OPERATION\n"
        "type=replace_text\n"
        "path=M256_PLAN_TARGET.TMP\n"
        "old_text=before\n"
        "new_text=after\n"
        "END_OPERATION\n";

    if (!openai_plan_save("m256 checkpoint goal", plan_text)) {
        (void)puts("M256 failed: unable to create checkpoint plan fixture.");
        cleanup();
        return EXIT_FAILURE;
    }

    /* First resume binds the active plan to the still-current session. */
    if (!openai_session_resume(id)) {
        (void)puts("M256 failed: unable to bind checkpoint on resume.");
        cleanup();
        return EXIT_FAILURE;
    }

    /* A later resume with unchanged fingerprints must remain valid. */
    if (!openai_session_resume(id)) {
        (void)puts("M256 failed: unchanged checkpoint did not resume.");
        cleanup();
        return EXIT_FAILURE;
    }

    /* Change the fingerprinted file; continuation must now be refused. */
    if (!write_target("changed\n")) {
        (void)puts("M256 failed: unable to mutate plan target fixture.");
        cleanup();
        return EXIT_FAILURE;
    }

    if (openai_session_resume(id)) {
        (void)puts("M256 failed: stale planned-file checkpoint resumed.");
        cleanup();
        return EXIT_FAILURE;
    }

    cleanup();
    (void)puts("M256 session resume reapproval/checkpoint evidence passed.");
    return EXIT_SUCCESS;
}
