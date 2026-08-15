/*
 * M256 Phase 5 guarded session-resume wrapper.
 *
 * Keep the mature session implementation intact while adding the Phase 5
 * safety rules for resumable work:
 *   - a resumed session never inherits an earlier saved-plan approval;
 *   - an active saved plan is bound to the session that was current when the
 *     plan was created/left active;
 *   - later resumes require the same plan checksum and current file
 *     fingerprints before continuation is allowed.
 */
#define openai_session_resume openai_sess_resume_base
#define openai_session_resume_cmd openai_sess_resume_cmd_base
#include "openai_session.c"
#undef openai_session_resume
#undef openai_session_resume_cmd

#include "openai_checkpoint.c"

static int m256_resume_id(const char *arguments, char id[9])
{
    const char *value;
    unsigned int index;
    unsigned char ch;

    if (arguments == NULL || id == NULL) return 0;
    value = arguments;
    while (*value == ' ' || *value == '\t') ++value;

    for (index = 0U; index < 8U; ++index) {
        ch = (unsigned char)value[index];
        if (!((ch >= '0' && ch <= '9') ||
              (ch >= 'A' && ch <= 'F') ||
              (ch >= 'a' && ch <= 'f'))) return 0;
        if (ch >= 'a' && ch <= 'f') ch = (unsigned char)(ch - 'a' + 'A');
        id[index] = (char)ch;
    }
    id[8] = '\0';
    return value[8] == '\0';
}

int openai_session_resume(const char *arguments)
{
    char target[9];
    char previous[9];
    char summary[256];
    int had_previous;
    int checkpoint;
    int may_bind;

    if (!m256_resume_id(arguments, target)) return 0;

    had_previous = openai_session_current_id(previous);
    may_bind = had_previous && strcmp(previous, target) == 0;
    summary[0] = '\0';

    checkpoint = openai_checkpoint_resume(
        target, may_bind, summary, sizeof(summary));

    if (checkpoint < 0) {
        if (summary[0] != '\0') (void)puts(summary);
        openai_plan_session_reset();
        return 0;
    }

    if (checkpoint == 0 || !openai_sess_resume_base(arguments)) {
        return 0;
    }

    /* Approval is intentionally process/session-local and must be reacquired. */
    openai_plan_session_reset();

    if (summary[0] != '\0') (void)puts(summary);
    return 1;
}

void openai_session_resume_cmd(const char *arguments)
{
    if (!openai_session_resume(arguments)) {
        (void)puts(
            "Unable to resume session. It may not exist, may be archived, "
            "or its saved-plan checkpoint may be stale."
        );
        return;
    }

    (void)puts("Session resumed.");
    (void)puts(
        "Saved-plan approval cleared; reapprove the current plan before execution."
    );
}
