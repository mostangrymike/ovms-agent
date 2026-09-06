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
#define llm_session_resume llm_sess_resume_base
#define llm_session_resume_cmd llm_sess_resume_cmd_base
#include "LLM_SESSION_CORE.INC"
#undef llm_session_resume
#undef llm_session_resume_cmd

#include "LLM_CHECKPOINT.INC"

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

static int m304_resume_policy(const char *id, char policy[16])
{
    llm_session_rec records[LLM_SESSION_MAX];
    unsigned int count;
    int index;

    if (id == NULL || policy == NULL) return 0;

    count = llm_session_load(records, LLM_SESSION_MAX);
    index = llm_session_find(records, count, id);
    if (index < 0 || records[index].archived) return 0;

    if (strcmp(records[index].policy, "read-only") != 0 &&
        strcmp(records[index].policy, "workspace") != 0 &&
        strcmp(records[index].policy, "full") != 0) {
        return 0;
    }

    (void)strncpy(policy, records[index].policy, 15U);
    policy[15] = '\0';
    return 1;
}

int llm_session_resume(const char *arguments)
{
    char target[9];
    char previous[9];
    char policy[16];
    char summary[256];
    int had_previous;
    int checkpoint;
    int may_bind;

    if (!m256_resume_id(arguments, target)) return 0;
    if (!m304_resume_policy(target, policy)) return 0;

    had_previous = llm_session_current_id(previous);
    may_bind = had_previous && strcmp(previous, target) == 0;
    summary[0] = '\0';

    checkpoint = llm_checkpoint_resume(
        target, may_bind, summary, sizeof(summary));

    if (checkpoint < 0) {
        if (summary[0] != '\0') (void)puts(summary);
        llm_plan_session_reset();
        return 0;
    }

    if (checkpoint == 0 || !llm_sess_resume_base(arguments)) {
        return 0;
    }

    /* Bind the active process policy to the validated persisted session. */
    if (!llm_set_approval(policy)) return 0;

    /* Saved-plan approval remains process/session-local and is reacquired. */
    llm_plan_session_reset();

    if (summary[0] != '\0') (void)puts(summary);
    return 1;
}

void llm_session_resume_cmd(const char *arguments)
{
    if (!llm_session_resume(arguments)) {
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