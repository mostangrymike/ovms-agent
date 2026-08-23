/* M268 outer-scope adapter for the existing AGENT/AUTO/LIMITS counters.
 * Nested agent runs keep consuming one shared turn/write budget instead of
 * resetting the counters for every repair-plan iteration.
 */
static int m268_auto_outer_active = 0;

void llm_auto_outer_begin(void)
{
    llm_auto_begin_base(LLM_WORKFLOW_WRITE);
    m268_auto_outer_active = 1;
}

void llm_auto_outer_end(const char *reason)
{
    const char *final_reason;

    final_reason = reason;
    if (final_reason == NULL || *final_reason == '\0') {
        final_reason = "unknown";
    }

    m268_auto_outer_active = 0;

    /* The repair executor owns rollback for source edits.  Finish the RMS
     * bookkeeping as a completed outer session, then retain the actual M268
     * terminal reason for AGENT/AUTO/STATUS.
     */
    llm_auto_finish_base("final");
    (void)strncpy(
        llm_auto_last_reason,
        final_reason,
        sizeof(llm_auto_last_reason) - 1U
    );
    llm_auto_last_reason[sizeof(llm_auto_last_reason) - 1U] = '\0';
}

void llm_auto_begin(int workflow)
{
    if (!m268_auto_outer_active) {
        llm_auto_begin_base(workflow);
    }
}

unsigned int llm_auto_turn_limit(int workflow)
{
    unsigned int limit;

    if (!m268_auto_outer_active) {
        return llm_auto_turn_limit_base(workflow);
    }

    limit = llm_auto_turn_cfg();
    if (llm_auto_cur_turns >= limit) {
        return 0U;
    }

    return limit - llm_auto_cur_turns;
}

void llm_auto_note_turn(void)
{
    llm_auto_note_turn_base();
}

void llm_auto_note_tool(void)
{
    llm_auto_note_tool_base();
}

int llm_auto_allow_write(void)
{
    return llm_auto_allow_write_base();
}

void llm_auto_finish(const char *reason)
{
    if (!m268_auto_outer_active) {
        llm_auto_finish_base(reason);
    }
}

int llm_auto_take_turns(unsigned int count)
{
    unsigned int limit;

    if (count == 0U) {
        return 1;
    }

    limit = llm_auto_turn_cfg();
    if (llm_auto_cur_turns > limit ||
        count > limit - llm_auto_cur_turns) {
        return 0;
    }

    llm_auto_cur_turns += count;
    return 1;
}

int llm_auto_take_writes(unsigned int count)
{
    unsigned int limit;

    if (count == 0U) {
        return 1;
    }

    limit = llm_auto_write_cfg();
    if (llm_auto_cur_writes > limit ||
        count > limit - llm_auto_cur_writes) {
        /* Refuse the whole candidate before any source write.  Do not charge
         * a partial reservation for a plan that will never execute.
         */
        llm_auto_write_blocked = 1;
        return 0;
    }

    llm_auto_cur_writes += count;
    return 1;
}

int llm_auto_budget_exhausted(void)
{
    return llm_auto_cur_turns >= llm_auto_turn_cfg() ||
           llm_auto_cur_writes >= llm_auto_write_cfg();
}

/* Focused regression hooks. */
int llm_m268_outer_active(void)
{
    return m268_auto_outer_active;
}

unsigned int llm_m268_turn_count(void)
{
    return llm_auto_cur_turns;
}

unsigned int llm_m268_write_count(void)
{
    return llm_auto_cur_writes;
}
