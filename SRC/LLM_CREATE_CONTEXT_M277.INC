/* M277 persistent-session guarded file-creation bridge. */

int llm_m277_create_allowed(int policy, int write_enabled)
{
    if (!write_enabled) {
        return 0;
    }

    return policy == LLM_APPROVAL_WORK ||
           policy == LLM_APPROVAL_FULL;
}

static int m277_goal_is_blank(const char *goal)
{
    const char *cursor;

    if (goal == NULL) {
        return 1;
    }

    cursor = goal;
    while (*cursor == ' ' || *cursor == '\t' ||
           *cursor == '\r' || *cursor == '\n') {
        ++cursor;
    }

    return *cursor == '\0';
}

void llm_exec_create_context(agent_state *state,
                             const char *goal)
{
    char model_goal[8192];
    int policy;

    if (state == NULL || m277_goal_is_blank(goal)) {
        (void)puts("Usage: AGENT/EXEC/RESUME/CREATE <goal>");
        return;
    }

    if (!llm_context_build(
            goal,
            model_goal,
            sizeof(model_goal))) {
        (void)puts("Unable to rebuild persistent session context.");
        return;
    }

    policy = llm_policy_source();

    if (policy == LLM_APPROVAL_READ) {
        (void)puts(
            "Persistent file creation requires workspace or full "
            "approval policy."
        );
        return;
    }

    if (!state->write_enabled) {
        (void)puts(
            "Persistent file creation requires "
            "OVMS_AGENT_WRITE_ENABLED."
        );
        return;
    }

    if (!llm_m277_create_allowed(policy, state->write_enabled)) {
        (void)puts("Persistent file creation approval policy is invalid.");
        return;
    }

    if (!llm_session_note_goal(goal)) {
        (void)puts("Unable to persist current session goal.");
        return;
    }

    llm_tx_note_exec("CREATE", goal, "started");
    llm_agent_create(state, model_goal);
}
