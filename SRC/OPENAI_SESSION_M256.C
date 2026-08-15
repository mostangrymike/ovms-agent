/*
 * M256 Phase 5 guarded session-resume wrapper.
 *
 * Keep the mature session implementation intact while adding the Phase 5
 * safety rule that a resumed session never inherits an earlier saved-plan
 * approval.  The base entry points are renamed only within this translation
 * unit; the public names remain unchanged for callers.
 */
#define openai_session_resume openai_sess_resume_base
#define openai_session_resume_cmd openai_sess_resume_cmd_base
#include "openai_session.c"
#undef openai_session_resume
#undef openai_session_resume_cmd

int openai_session_resume(const char *arguments)
{
    if (!openai_sess_resume_base(arguments)) {
        return 0;
    }

    /* Approval is intentionally process/session-local and must be reacquired. */
    openai_plan_session_reset();
    return 1;
}

void openai_session_resume_cmd(const char *arguments)
{
    if (!openai_session_resume(arguments)) {
        (void)puts(
            "Unable to resume session. It may not exist or may be archived."
        );
        return;
    }

    (void)puts("Session resumed.");
    (void)puts(
        "Saved-plan approval cleared; reapprove the current plan before execution."
    );
}
