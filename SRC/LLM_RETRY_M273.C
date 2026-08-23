#include <stdio.h>

#include "project.h"

static int (*m273_retry_gitdiff_fn)(void) = NULL;

void m273_retry_set_gitdiff(int (*callback)(void))
{
    m273_retry_gitdiff_fn = callback;
}

static void m273_repair_git_diff(const agent_state *state)
{
    int status;

    if (m273_retry_gitdiff_fn == NULL) {
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

    status = m273_retry_gitdiff_fn();
    if ((status & 1) == 0) {
        (void)printf(
            "Git diff failed with OpenVMS status %d.\n",
            status
        );
    }
}

#define project_git_diff m273_repair_git_diff
#include "LLM_RETRY.C"
#undef project_git_diff
