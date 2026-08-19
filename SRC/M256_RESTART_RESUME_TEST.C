#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "llm_internal.h"

#define TEST_DATA "M256_RST_SESSIONS.DAT"
#define TEST_CUR  "M256_RST_SESSION.CUR"
#define TEST_ID   "M256_RST_SESSION.ID"
#define TARGET    "TEST/M256_RESTART_TARGET.TMP"

extern int openai_plan_approved;
extern unsigned long openai_approved_hash;
extern int openai_approval_invalidated;

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
    FILE *file;
    char id[16];
    char *end;
    char output[4096];

    openai_test_session_paths(TEST_DATA, TEST_CUR);

    file = fopen(TEST_ID, "r");
    if (file == NULL || fgets(id, sizeof(id), file) == NULL) {
        if (file != NULL) (void)fclose(file);
        (void)puts("M256 restart resume failed: persisted session id missing.");
        cleanup();
        return EXIT_FAILURE;
    }
    (void)fclose(file);

    end = strchr(id, '\n');
    if (end != NULL) *end = '\0';
    end = strchr(id, '\r');
    if (end != NULL) *end = '\0';

    if (!openai_session_resume(id) ||
        !openai_session_show_text(id, output, sizeof(output)) ||
        strstr(output, "Original goal: resume parser repair after restart") == NULL ||
        strstr(output, "Current goal:  resume parser repair after restart") == NULL) {
        (void)puts("M256 restart resume failed: session objective/checkpoint restore.");
        cleanup();
        return EXIT_FAILURE;
    }

    if (openai_plan_approved != 0 ||
        openai_approved_hash != 0UL ||
        openai_approval_invalidated != 0) {
        (void)puts("M256 restart resume failed: approval state was reused.");
        cleanup();
        return EXIT_FAILURE;
    }

    cleanup();
    (void)puts("M256 cross-process restart/resume evidence passed.");
    return EXIT_SUCCESS;
}
