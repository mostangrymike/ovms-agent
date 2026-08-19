#include <string.h>

#define openai_tool_run openai_tool_run_base
#define openai_tool_run_cmd openai_tool_run_cmd_base
#include "openai_transcript.c"
#undef openai_tool_run
#undef openai_tool_run_cmd

#include "git_rms_restore.h"

#define M265_GIT_BATCH_MAX 32U

static int m265_git_path_safe(const char *path)
{
    const unsigned char *cursor;

    if (path == NULL || *path == '\0' || path[0] == '-' ||
        strstr(path, "..") != NULL) {
        return 0;
    }

    cursor = (const unsigned char *)path;
    while (*cursor != '\0') {
        if (!((*cursor >= (unsigned char)'A' &&
               *cursor <= (unsigned char)'Z') ||
              (*cursor >= (unsigned char)'a' &&
               *cursor <= (unsigned char)'z') ||
              (*cursor >= (unsigned char)'0' &&
               *cursor <= (unsigned char)'9') ||
              *cursor == (unsigned char)'_' ||
              *cursor == (unsigned char)'-' ||
              *cursor == (unsigned char)'.' ||
              *cursor == (unsigned char)'/' ||
              *cursor == (unsigned char)'$')) {
            return 0;
        }
        ++cursor;
    }

    return 1;
}

static int m265_git_batch_split(
    const char *text,
    char *buffer,
    size_t buffer_size,
    char **paths,
    unsigned int *count)
{
    char *cursor;
    char *start;
    char *end;
    unsigned int used;
    size_t length;

    if (text == NULL || buffer == NULL || buffer_size == 0U ||
        paths == NULL || count == NULL) {
        return 0;
    }

    length = strlen(text);
    if (length == 0U || length >= buffer_size) {
        return 0;
    }

    (void)strcpy(buffer, text);
    cursor = buffer;
    used = 0U;

    while (*cursor != '\0') {
        while (*cursor == ' ' || *cursor == '\t') {
            ++cursor;
        }

        start = cursor;
        while (*cursor != '\0' && *cursor != ',') {
            ++cursor;
        }
        end = cursor;

        while (end > start &&
               (end[-1] == ' ' || end[-1] == '\t')) {
            --end;
        }

        if (end == start || used >= M265_GIT_BATCH_MAX) {
            return 0;
        }

        *end = '\0';
        if (!m265_git_path_safe(start)) {
            return 0;
        }

        paths[used++] = start;

        if (*cursor == ',') {
            *cursor = '\0';
            ++cursor;
            if (*cursor == '\0') {
                return 0;
            }
        }
    }

    *count = used;
    return used > 0U;
}

static int m265_git_restore_tool(
    agent_state *state,
    const char *path_text)
{
    char answer[32];
    char buffer[OPENAI_TRANSCRIPT_ARG];
    char *paths[M265_GIT_BATCH_MAX];
    unsigned int count;
    unsigned int index;

    if (state == NULL || path_text == NULL || *path_text == '\0') {
        return 0;
    }

    if (openai_tx_policy_level() < OPENAI_TOOL_WORK) {
        (void)openai_tx_append(
            "tool", "GITRESTORE", "write", "denied", path_text
        );
        return 0;
    }

    if (!state->write_enabled) {
        (void)openai_tx_append(
            "tool", "GITRESTORE", "write", "write-gate", path_text
        );
        return 0;
    }

    count = 0U;
    if (!m265_git_batch_split(
            path_text,
            buffer,
            sizeof(buffer),
            paths,
            &count)) {
        (void)puts(
            "GITRESTORE expects one safe path or a comma-separated "
            "list of up to 32 safe paths."
        );
        return 1;
    }

    for (index = 0U; index < count; ++index) {
        if (!openai_tx_append(
                "tool", "GITRESTORE", "write", "dispatched", paths[index])) {
            return 0;
        }
    }

    if (count == 1U) {
        (void)printf(
            "Restore %s from Git HEAD as a new OpenVMS RMS version [y/N]? ",
            paths[0]
        );
    } else {
        (void)printf(
            "Restore %u files from Git HEAD as new OpenVMS RMS versions [y/N]? ",
            count
        );
    }
    (void)fflush(stdout);

    if (fgets(answer, sizeof(answer), stdin) == NULL ||
        !(answer[0] == 'y' || answer[0] == 'Y')) {
        (void)puts("Git restore declined.");
        for (index = 0U; index < count; ++index) {
            (void)openai_tx_append(
                "tool", "GITRESTORE", "write", "declined", paths[index]
            );
        }
        return 1;
    }

    for (index = 0U; index < count; ++index) {
        if (!git_rms_restore_head(paths[index])) {
            (void)printf(
                "RMS-aware Git restore failed for %s; effective file "
                "contents were not verified against HEAD.\n",
                paths[index]
            );
            (void)openai_tx_append(
                "tool", "GITRESTORE", "write", "error", paths[index]
            );
            if (count > 1U) {
                (void)printf(
                    "Batch restore stopped after %u of %u files.\n",
                    index,
                    count
                );
            }
            return 1;
        }

        (void)openai_tx_append(
            "tool", "GITRESTORE", "write", "applied", paths[index]
        );

        if (count > 1U) {
            (void)printf("Restored and verified: %s\n", paths[index]);
        }
    }

    if (count == 1U) {
        (void)puts(
            "Git HEAD contents restored and verified as the effective "
            "OpenVMS file version."
        );
    } else {
        (void)printf(
            "Batch Git restore completed: %u files restored and verified.\n",
            count
        );
    }

    return 1;
}

int openai_tool_run(agent_state *state,
                    const char *arguments)
{
    char name[32];
    char rest[OPENAI_TRANSCRIPT_ARG];

    if (state != NULL &&
        openai_tx_split(arguments,
                        name, sizeof(name),
                        rest, sizeof(rest)) &&
        openai_tx_equal_ci(name, "GITRESTORE")) {
        return m265_git_restore_tool(state, rest);
    }

    return openai_tool_run_base(state, arguments);
}

void openai_tool_run_cmd(agent_state *state,
                         const char *arguments)
{
    if (!openai_tool_run(state, arguments)) {
        (void)puts(
            "AGENT/TOOL/RUN refused or invalid. "
            "Use AGENT/TOOLS for supported capabilities."
        );
    }
}
