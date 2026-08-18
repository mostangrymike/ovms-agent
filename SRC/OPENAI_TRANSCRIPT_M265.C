#define openai_tool_run openai_tool_run_base
#define openai_tool_run_cmd openai_tool_run_cmd_base
#include "openai_transcript.c"
#undef openai_tool_run
#undef openai_tool_run_cmd

#include "git_rms_restore.h"

static int m265_git_restore_tool(
    agent_state *state,
    const char *path)
{
    char answer[32];

    if (state == NULL || path == NULL || *path == '\0') {
        return 0;
    }

    if (openai_tx_policy_level() < OPENAI_TOOL_WORK) {
        (void)openai_tx_append(
            "tool", "GITRESTORE", "write", "denied", path
        );
        return 0;
    }

    if (!state->write_enabled) {
        (void)openai_tx_append(
            "tool", "GITRESTORE", "write", "write-gate", path
        );
        return 0;
    }

    if (!openai_tx_append(
            "tool", "GITRESTORE", "write", "dispatched", path)) {
        return 0;
    }

    (void)printf(
        "Restore %s from Git HEAD as a new OpenVMS RMS version [y/N]? ",
        path
    );
    (void)fflush(stdout);

    if (fgets(answer, sizeof(answer), stdin) == NULL ||
        !(answer[0] == 'y' || answer[0] == 'Y')) {
        (void)puts("Git restore declined.");
        (void)openai_tx_append(
            "tool", "GITRESTORE", "write", "declined", path
        );
        return 1;
    }

    if (!git_rms_restore_head(path)) {
        (void)puts(
            "RMS-aware Git restore failed; effective file contents "
            "were not verified against HEAD."
        );
        (void)openai_tx_append(
            "tool", "GITRESTORE", "write", "error", path
        );
        return 1;
    }

    (void)puts(
        "Git HEAD contents restored and verified as the effective "
        "OpenVMS file version."
    );
    (void)openai_tx_append(
        "tool", "GITRESTORE", "write", "applied", path
    );
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
