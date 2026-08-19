#include <stdio.h>
#include <stdlib.h>

#include "llm_internal.h"
#include "openai_plan.h"

#define TEST_DATA "M256_RST_SESSIONS.DAT"
#define TEST_CUR  "M256_RST_SESSION.CUR"
#define TEST_ID   "M256_RST_SESSION.ID"
#define TARGET    "TEST/M256_RESTART_TARGET.TMP"

int command_line_complete(const char *input,
                          size_t input_size,
                          int reached_eof)
{
    (void)input; (void)input_size; (void)reached_eof;
    return 0;
}

int command_read_stream(FILE *stream,
                        char *input,
                        size_t input_size)
{
    (void)stream; (void)input; (void)input_size;
    return 0;
}

void openai_test_ckpt_clear(void);

static void remove_all(const char *path)
{
    while (remove(path) == 0) { }
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

static void cleanup(void)
{
    openai_test_session_paths(NULL, NULL);
    openai_plan_session_reset();
    openai_test_ckpt_clear();
    remove_all(TEST_DATA);
    remove_all(TEST_CUR);
    remove_all(TEST_ID);
    remove_all(TARGET);
    remove_all("OVMS_AGENT_PLAN.TXT");
    remove_all("OVMS_AGENT_PLAN.TXT.CHK");
}

int main(void)
{
    char id[9];
    FILE *file;
    const char *plan_text;

    cleanup();
    openai_test_session_paths(TEST_DATA, TEST_CUR);

    if (!openai_session_new("m256 restart session", id) ||
        !openai_session_note_goal("resume parser repair after restart") ||
        !write_text(TARGET, "before\n")) {
        (void)puts("M256 restart setup failed: session/goal/target fixture.");
        cleanup();
        return EXIT_FAILURE;
    }

    plan_text =
        "Files to modify\n"
        "TEST/M256_RESTART_TARGET.TMP\n"
        "operation_count=1\n"
        "BEGIN_OPERATION\n"
        "type=replace_text\n"
        "path=TEST/M256_RESTART_TARGET.TMP\n"
        "old_text=before\n"
        "new_text=after\n"
        "END_OPERATION\n";

    if (!openai_plan_save("resume parser repair after restart", plan_text) ||
        !openai_session_resume(id)) {
        (void)puts("M256 restart setup failed: plan/checkpoint binding.");
        cleanup();
        return EXIT_FAILURE;
    }

    file = fopen(TEST_ID, "w");
    if (file == NULL || fprintf(file, "%s\n", id) < 0 || fclose(file) != 0) {
        if (file != NULL) (void)fclose(file);
        (void)puts("M256 restart setup failed: session id persistence.");
        cleanup();
        return EXIT_FAILURE;
    }

    openai_test_session_paths(NULL, NULL);
    (void)puts("M256 restart setup evidence passed.");
    return EXIT_SUCCESS;
}
