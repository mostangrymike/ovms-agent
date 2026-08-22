static void llm_m132_save_current(
    int *last_workflow,
    int *last_build_known,
    int *last_build_status,
    int *last_rollback,
    int *saved_workflow,
    int *saved_build_known,
    int *saved_build_status,
    int *saved_rollback,
    int *valid)
{
    *last_workflow = llm_last_workflow;
    *last_build_known = llm_last_build_known;
    *last_build_status = llm_last_build_status;
    *last_rollback = llm_last_rollback;
    *saved_workflow = llm_saved_workflow;
    *saved_build_known = llm_saved_build_known;
    *saved_build_status = llm_saved_build_status;
    *saved_rollback = llm_saved_rollback;
    *valid = llm_state_valid;
}

static void llm_m132_restore_current(
    int last_workflow,
    int last_build_known,
    int last_build_status,
    int last_rollback,
    int saved_workflow,
    int saved_build_known,
    int saved_build_status,
    int saved_rollback,
    int valid)
{
    llm_last_workflow = last_workflow;
    llm_last_build_known = last_build_known;
    llm_last_build_status = last_build_status;
    llm_last_rollback = last_rollback;
    llm_saved_workflow = saved_workflow;
    llm_saved_build_known = saved_build_known;
    llm_saved_build_status = saved_build_status;
    llm_saved_rollback = saved_rollback;
    llm_state_valid = valid;
}

int llm_m132_clear_current_work(
    const char *path)
{
    int lw;
    int lbk;
    int lbs;
    int lr;
    int sw;
    int sbk;
    int sbs;
    int sr;
    int valid;
    int ok;

    llm_m132_save_current(
        &lw, &lbk, &lbs, &lr,
        &sw, &sbk, &sbs, &sr,
        &valid);

    (void)llm_state_purge_all(path);

    llm_last_workflow = LLM_WORKFLOW_REVIEW;
    llm_last_build_known = 1;
    llm_last_build_status = 41;
    llm_last_rollback = LLM_ROLLBACK_SUCCEEDED;

    ok =
        !llm_state_apply_load(path) &&
        llm_state_valid == 0 &&
        llm_last_workflow == LLM_WORKFLOW_NONE;

    llm_m132_restore_current(
        lw, lbk, lbs, lr,
        sw, sbk, sbs, sr,
        valid);

    return ok;
}

int llm_m132_clear_current_build(
    const char *path)
{
    int lw;
    int lbk;
    int lbs;
    int lr;
    int sw;
    int sbk;
    int sbs;
    int sr;
    int valid;
    int ok;

    llm_m132_save_current(
        &lw, &lbk, &lbs, &lr,
        &sw, &sbk, &sbs, &sr,
        &valid);

    (void)llm_state_purge_all(path);

    llm_last_workflow = LLM_WORKFLOW_EXECUTE;
    llm_last_build_known = 1;
    llm_last_build_status = 43;
    llm_last_rollback = LLM_ROLLBACK_SUCCEEDED;

    ok =
        !llm_state_apply_load(path) &&
        llm_last_build_known == 0 &&
        llm_last_build_status == 0;

    llm_m132_restore_current(
        lw, lbk, lbs, lr,
        sw, sbk, sbs, sr,
        valid);

    return ok;
}

int llm_m132_clear_current_roll(
    const char *path)
{
    int lw;
    int lbk;
    int lbs;
    int lr;
    int sw;
    int sbk;
    int sbs;
    int sr;
    int valid;
    int ok;

    llm_m132_save_current(
        &lw, &lbk, &lbs, &lr,
        &sw, &sbk, &sbs, &sr,
        &valid);

    (void)llm_state_purge_all(path);

    llm_last_workflow = LLM_WORKFLOW_EXECUTE;
    llm_last_build_known = 1;
    llm_last_build_status = 45;
    llm_last_rollback = LLM_ROLLBACK_SUCCEEDED;

    ok =
        !llm_state_apply_load(path) &&
        llm_last_rollback == LLM_ROLLBACK_NONE;

    llm_m132_restore_current(
        lw, lbk, lbs, lr,
        sw, sbk, sbs, sr,
        valid);

    return ok;
}

int llm_m132_fail_keeps_current(void)
{
    int lw;
    int lbk;
    int lbs;
    int lr;
    int sw;
    int sbk;
    int sbs;
    int sr;
    int valid;
    int prior_valid;
    int ok;

    llm_m132_save_current(
        &lw, &lbk, &lbs, &lr,
        &sw, &sbk, &sbs, &sr,
        &valid);

    llm_last_workflow = LLM_WORKFLOW_REVIEW;
    llm_last_build_known = 1;
    llm_last_build_status = 47;
    llm_last_rollback = LLM_ROLLBACK_NONE;
    llm_state_valid = 1;

    prior_valid = llm_state_valid;
    llm_state_note_save_result(0);
    llm_state_valid = prior_valid;

    ok =
        llm_last_workflow == LLM_WORKFLOW_REVIEW &&
        llm_last_build_known == 1 &&
        llm_last_build_status == 47 &&
        llm_last_rollback == LLM_ROLLBACK_NONE;

    llm_m132_restore_current(
        lw, lbk, lbs, lr,
        sw, sbk, sbs, sr,
        valid);

    return ok;
}
