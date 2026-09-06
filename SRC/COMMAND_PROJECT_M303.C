#include <stdio.h>
#include <string.h>

#include "project.h"
#include "COMMAND_BUILD_GATE.INC"

static int (*m273_cmd_gitdiff_fn)(void) = NULL;
static const char *(*m303_approval_fn)(void) = NULL;

void m273_cmd_set_gitdiff(int (*callback)(void))
{
    m273_cmd_gitdiff_fn = callback;
}

void m303_cmd_set_approval(
    const char *(*callback)(void))
{
    m303_approval_fn = callback;
}

static void m273_cmd_git_diff(const agent_state *state)
{
    int status;

    if (m273_cmd_gitdiff_fn == NULL) {
        project_git_diff(state);
        return;
    }

    if (state == NULL ||
        state->project_root == NULL ||
        *state->project_root == '\0') {
        (void)puts("OVMS_AGENT_ROOT is not defined.");
        return;
    }

    (void)puts("Git diff:");
    (void)puts("");

    status = m273_cmd_gitdiff_fn();
    if ((status & 1) == 0) {
        (void)printf(
            "Git diff failed with OpenVMS status %d.\n",
            status
        );
    }
}

#define project_git_diff m273_cmd_git_diff
#define project_commands m303_project_commands_raw
#define command_build m303_command_build_raw
#define command_register_project m303_register_raw
#include "COMMAND_PROJECT.C"
#undef command_register_project
#undef command_build
#undef project_commands
#undef project_git_diff

void command_build(agent_state *state,
                   const char *arguments)
{
    const char *approval;
    int gate;

    if (state == NULL ||
        state->project_root == NULL ||
        *state->project_root == '\0') {
        m303_command_build_raw(state, arguments);
        return;
    }

    approval = m303_approval_fn != NULL ?
        m303_approval_fn() : NULL;
    gate = m303_build_gate(state, approval);

    if (gate == M303_BUILD_GATE_POLICY) {
        (void)puts(
            "BUILD refused: workspace approval policy required."
        );
        return;
    }

    if (gate == M303_BUILD_GATE_WRITE) {
        (void)puts(
            "BUILD refused: guarded writes are disabled."
        );
        return;
    }

    if (gate == M303_BUILD_GATE_DCL) {
        (void)puts(
            "BUILD refused: DCL execution is disabled."
        );
        return;
    }

    m303_command_build_raw(state, arguments);
}

static const command_entry m303_project_commands[] = {
    { "ROOT", "Display the project root", command_root },
    { "STATUS", "Display agent status", command_status },
    { "LIST", "List a directory: LIST [path]", command_list },
    { "TREE", "Display directory tree: TREE [path]", command_tree },
    { "READ", "Read lines: READ file [start [count]]", command_read },
    { "BUILD", "Build the current project", command_build },
    { "GITSTATUS", "Display Git status", command_gitstatus },
    { "GITDIFF", "Display uncommitted source changes", command_gitdiff },
    { "EDIT", "Edit a file: EDIT file", command_edit },
    { "GREP", "Search project files: GREP \"text\" [path] [/CONTEXT=n] [/LIMIT=n] [/CASE=value] [/COUNT] [/FILES] "
            "[/NAME=pattern] [/EXCLUDE=pattern] [/DEPTH=n] [/WORD]", command_grep },
    { "SEARCH", "Search a file: SEARCH file \"text\"", command_search },
    { "PATCH", "Replace exact text: PATCH file \"old\" \"new\"", command_patch }
};

void command_register_project(void)
{
    (void)command_registry_add(
        m303_project_commands,
        sizeof(m303_project_commands) /
            sizeof(m303_project_commands[0])
    );
}
