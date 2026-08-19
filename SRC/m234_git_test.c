#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "llm_internal.h"

int openai_git_rms_copy(const char *path, const char *target);

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

static void m263_remove_versions(const char *path)
{
    while (remove(path) == 0) {
    }
}

static int m263_write_version(const char *path, const char *text)
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

static int m263_expect_text(const char *path, const char *expected)
{
    FILE *file;
    char line[128];
    size_t length;

    file = fopen(path, "r");
    if (file == NULL) {
        return 0;
    }
    line[0] = '\0';
    if (fgets(line, sizeof(line), file) == NULL) {
        (void)fclose(file);
        return 0;
    }
    (void)fclose(file);

    length = strlen(line);
    while (length > 0U &&
           (line[length - 1U] == '\n' || line[length - 1U] == '\r')) {
        line[--length] = '\0';
    }
    return strcmp(line, expected) == 0;
}

int main(void)
{
    agent_state state;
    char output[32768];
    const char *rms_path;
    const char *copy_path;

    (void)memset(&state, 0, sizeof(state));
    state.project_root = ".";

    openai_test_git_data(
        " M SRC/OPENAI_AGENT.C\n"
        "?? SRC/OPENAI_GIT_CONTEXT.C",
        "diff --git a/SRC/OPENAI_AGENT.C b/SRC/OPENAI_AGENT.C\n"
        "+    git context enabled"
    );

    if (!openai_git_refresh(&state)) {
        (void)puts("M234 failed: Git refresh.");
        return EXIT_FAILURE;
    }

    if (!openai_git_status_text(
            &state, output, sizeof(output)) ||
        strstr(output, "Changed paths: 2") == NULL ||
        strstr(output, "OPENAI_AGENT.C") == NULL ||
        strstr(output, "OPENAI_GIT_CONTEXT.C") == NULL) {
        (void)puts("M234 failed: Git status context.");
        return EXIT_FAILURE;
    }

    if (!openai_git_diff_text(
            &state, output, sizeof(output)) ||
        strstr(output, "git context enabled") == NULL) {
        (void)puts("M234 failed: Git diff context.");
        return EXIT_FAILURE;
    }

    if (!openai_git_changed_text(
            &state, output, sizeof(output)) ||
        strstr(output, "Count: 2") == NULL) {
        (void)puts("M234 failed: changed path view.");
        return EXIT_FAILURE;
    }

    if (!openai_git_compose(
            &state,
            "Review the current edits.",
            output,
            sizeof(output)) ||
        strstr(output, "GIT WORKING TREE") == NULL ||
        strstr(output, "UNSTAGED DIFF") == NULL ||
        strstr(output, "Review the current edits.") == NULL) {
        (void)puts("M234 failed: Git context composition.");
        return EXIT_FAILURE;
    }

    if (!openai_parity_text(output, sizeof(output)) ||
        strstr(output, "Git state context:     available") == NULL ||
        strstr(output, "Git diff awareness:    available") == NULL) {
        (void)puts("M234 failed: parity status.");
        return EXIT_FAILURE;
    }

    openai_test_git_data(NULL, NULL);

    rms_path = "M263_RMS_GIT.TMP";
    copy_path = "M263_RMS_COPY.TMP";
    m263_remove_versions(rms_path);
    m263_remove_versions(copy_path);

    if (!m263_write_version(rms_path, "baseline\n") ||
        !m263_write_version(rms_path, "version two\n") ||
        !m263_write_version(rms_path, "newest version three\n")) {
        (void)puts("M263 failed: unable to create RMS versions.");
        m263_remove_versions(rms_path);
        return EXIT_FAILURE;
    }

    if (!openai_git_rms_copy(rms_path, copy_path) ||
        !m263_expect_text(copy_path, "newest version three")) {
        (void)puts("M263 failed: RMS Git view did not copy newest version.");
        m263_remove_versions(rms_path);
        m263_remove_versions(copy_path);
        return EXIT_FAILURE;
    }

    if (!m263_expect_text("M263_RMS_GIT.TMP;1", "baseline") ||
        !m263_expect_text("M263_RMS_GIT.TMP;2", "version two") ||
        !m263_expect_text("M263_RMS_GIT.TMP;3", "newest version three")) {
        (void)puts("M263 failed: older RMS versions were not preserved.");
        m263_remove_versions(rms_path);
        m263_remove_versions(copy_path);
        return EXIT_FAILURE;
    }

    m263_remove_versions(rms_path);
    m263_remove_versions(copy_path);

    (void)puts("Git-aware autonomous context bundle and RMS view test passed.");
    return EXIT_SUCCESS;
}
