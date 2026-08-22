static void openai_m132_save_current(
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
    *last_workflow = openai_last_workflow;
    *last_build_known = openai_last_build_known;
    *last_build_status = openai_last_build_status;
    *last_rollback = openai_last_rollback;
    *saved_workflow = openai_saved_workflow;
    *saved_build_known = openai_saved_build_known;
    *saved_build_status = openai_saved_build_status;
    *saved_rollback = openai_saved_rollback;
    *valid = openai_state_valid;
}

static void openai_m132_restore_current(
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
    openai_last_workflow = last_workflow;
    openai_last_build_known = last_build_known;
    openai_last_build_status = last_build_status;
    openai_last_rollback = last_rollback;
    openai_saved_workflow = saved_workflow;
    openai_saved_build_known = saved_build_known;
    openai_saved_build_status = saved_build_status;
    openai_saved_rollback = saved_rollback;
    openai_state_valid = valid;
}

int openai_m132_clear_current_work(
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

    openai_m132_save_current(
        &lw, &lbk, &lbs, &lr,
        &sw, &sbk, &sbs, &sr,
        &valid);

    (void)openai_state_purge_all(path);

    openai_last_workflow = OPENAI_WORKFLOW_REVIEW;
    openai_last_build_known = 1;
    openai_last_build_status = 41;
    openai_last_rollback = OPENAI_ROLLBACK_SUCCEEDED;

    ok =
        !openai_state_apply_load(path) &&
        openai_state_valid == 0 &&
        openai_last_workflow == OPENAI_WORKFLOW_NONE;

    openai_m132_restore_current(
        lw, lbk, lbs, lr,
        sw, sbk, sbs, sr,
        valid);

    return ok;
}

int openai_m132_clear_current_build(
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

    openai_m132_save_current(
        &lw, &lbk, &lbs, &lr,
        &sw, &sbk, &sbs, &sr,
        &valid);

    (void)openai_state_purge_all(path);

    openai_last_workflow = OPENAI_WORKFLOW_EXECUTE;
    openai_last_build_known = 1;
    openai_last_build_status = 43;
    openai_last_rollback = OPENAI_ROLLBACK_SUCCEEDED;

    ok =
        !openai_state_apply_load(path) &&
        openai_last_build_known == 0 &&
        openai_last_build_status == 0;

    openai_m132_restore_current(
        lw, lbk, lbs, lr,
        sw, sbk, sbs, sr,
        valid);

    return ok;
}

int openai_m132_clear_current_roll(
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

    openai_m132_save_current(
        &lw, &lbk, &lbs, &lr,
        &sw, &sbk, &sbs, &sr,
        &valid);

    (void)openai_state_purge_all(path);

    openai_last_workflow = OPENAI_WORKFLOW_EXECUTE;
    openai_last_build_known = 1;
    openai_last_build_status = 45;
    openai_last_rollback = OPENAI_ROLLBACK_SUCCEEDED;

    ok =
        !openai_state_apply_load(path) &&
        openai_last_rollback == OPENAI_ROLLBACK_NONE;

    openai_m132_restore_current(
        lw, lbk, lbs, lr,
        sw, sbk, sbs, sr,
        valid);

    return ok;
}

int openai_m132_fail_keeps_current(void)
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

    openai_m132_save_current(
        &lw, &lbk, &lbs, &lr,
        &sw, &sbk, &sbs, &sr,
        &valid);

    openai_last_workflow = OPENAI_WORKFLOW_REVIEW;
    openai_last_build_known = 1;
    openai_last_build_status = 47;
    openai_last_rollback = OPENAI_ROLLBACK_NONE;
    openai_state_valid = 1;

    prior_valid = openai_state_valid;
    openai_state_note_save_result(0);
    openai_state_valid = prior_valid;

    ok =
        openai_last_workflow == OPENAI_WORKFLOW_REVIEW &&
        openai_last_build_known == 1 &&
        openai_last_build_status == 47 &&
        openai_last_rollback == OPENAI_ROLLBACK_NONE;

    openai_m132_restore_current(
        lw, lbk, lbs, lr,
        sw, sbk, sbs, sr,
        valid);

    return ok;
}
