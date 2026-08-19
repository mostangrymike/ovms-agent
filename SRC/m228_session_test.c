#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "llm_internal.h"

#define TEST_DATA "M228_SESSIONS.DAT"
#define TEST_CUR  "M228_SESSION.CUR"

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
    openai_test_session_paths(NULL, NULL);
    openai_test_reset_approval();
    remove_all(TEST_DATA);
    remove_all(TEST_CUR);
}

int main(void)
{
    char first[9];
    char forked[9];
    char output[8192];

    cleanup();
    openai_test_session_paths(TEST_DATA, TEST_CUR);

    if (!openai_session_new("alpha session", first) ||
        strlen(first) != 8U ||
        !openai_session_current_text(output, sizeof(output)) ||
        strstr(output, first) == NULL ||
        strstr(output, "alpha session") == NULL) {
        (void)puts("M228 failed: session creation/current.");
        cleanup();
        return EXIT_FAILURE;
    }

    if (!openai_set_approval("workspace") ||
        !openai_session_note_goal("fix parser and build") ||
        !openai_session_show_text(first, output, sizeof(output)) ||
        strstr(output, "Original goal: fix parser and build") == NULL ||
        strstr(output, "Current goal:  fix parser and build") == NULL ||
        strstr(output, "Policy:        workspace") == NULL ||
        strstr(output, "Executions:    1") == NULL) {
        (void)puts("M228 failed: session execution metadata.");
        cleanup();
        return EXIT_FAILURE;
    }

    {
        char args[128];

        (void)snprintf(args, sizeof(args), "%s beta fork", first);

        if (!openai_session_fork(args, forked) ||
            strcmp(first, forked) == 0 ||
            !openai_session_show_text(forked, output, sizeof(output)) ||
            strstr(output, "Name:          beta fork") == NULL ||
            strstr(output, first) == NULL ||
            strstr(output, "Executions:    1") == NULL) {
            (void)puts("M228 failed: session fork.");
            cleanup();
            return EXIT_FAILURE;
        }
    }

    {
        char args[128];

        (void)snprintf(args, sizeof(args), "%s renamed fork", forked);

        if (!openai_session_rename(args) ||
            !openai_session_show_text(forked, output, sizeof(output)) ||
            strstr(output, "Name:          renamed fork") == NULL) {
            (void)puts("M228 failed: session rename.");
            cleanup();
            return EXIT_FAILURE;
        }
    }

    if (!openai_session_list_text(output, sizeof(output)) ||
        strstr(output, "Sessions: 2") == NULL ||
        strstr(output, first) == NULL ||
        strstr(output, forked) == NULL) {
        (void)puts("M228 failed: session list.");
        cleanup();
        return EXIT_FAILURE;
    }

    if (!openai_session_archive(forked) ||
        openai_session_resume(forked) ||
        !openai_session_current_text(output, sizeof(output)) ||
        strstr(output, "Current session: none") == NULL ||
        !openai_session_unarchive(forked) ||
        !openai_session_resume(forked) ||
        !openai_session_current_text(output, sizeof(output)) ||
        strstr(output, forked) == NULL) {
        (void)puts("M228 failed: archive/resume lifecycle.");
        cleanup();
        return EXIT_FAILURE;
    }

    if (!openai_session_resume(first) ||
        !openai_session_delete(first) ||
        !openai_session_current_text(output, sizeof(output)) ||
        strstr(output, "Current session: none") == NULL ||
        openai_session_show_text(first, output, sizeof(output)) ||
        !openai_session_list_text(output, sizeof(output)) ||
        strstr(output, "Sessions: 1") == NULL ||
        strstr(output, forked) == NULL) {
        (void)puts("M228 failed: session delete lifecycle.");
        cleanup();
        return EXIT_FAILURE;
    }

    if (!openai_parity_text(output, sizeof(output)) ||
        strstr(output, "Persistent sessions:  available") == NULL ||
        strstr(output, "Session resume/fork:  available") == NULL ||
        strstr(output, "Archive lifecycle:    available") == NULL) {
        (void)puts("M228 failed: parity status.");
        cleanup();
        return EXIT_FAILURE;
    }

    cleanup();
    (void)puts("Persistent session parity bundle test passed.");
    return EXIT_SUCCESS;
}
