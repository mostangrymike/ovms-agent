#include "project.h"
#include "ANSI_TERM.H"

static void m273_repair_git_diff(const agent_state *state)
{
    int status;

    if (state == NULL ||
        state->project_root == NULL ||
        *state->project_root == '\0') {
        ansi_term_puts("OVMS_AGENT_ROOT is not defined.");
        return;
    }

    ansi_term_puts("Git diff:");
    ansi_term_puts("");

    status = ansi_term_git_diff();
    if ((status & 1) == 0) {
        ansi_term_printf(
            "Git diff failed with OpenVMS status %d.\n",
            status
        );
    }
}

#define project_git_diff m273_repair_git_diff
#include "LLM_RETRY.C"
#undef project_git_diff
