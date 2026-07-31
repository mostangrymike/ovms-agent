static void openai_m133_save_all(
    int *last_workflow,
    int *last_build_known,
    int *last_build_status,
    int *last_rollback,
    int *saved_workflow,
    int *saved_build_known,
    int *saved_build_status,
    int *saved_rollback,
    int *valid,
    int *recovered,
    int *save_known,
    int *save_succeeded)
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
    *recovered = openai_state_recovered;
    *save_known = openai_state_save_known;
    *save_succeeded = openai_state_save_succeeded;
}

static void openai_m133_restore_all(
    int last_workflow,
    int last_build_known,
    int last_build_status,
    int last_rollback,
    int saved_workflow,
    int saved_build_known,
    int saved_build_status,
    int saved_rollback,
    int valid,
    int recovered,
    int save_known,
    int save_succeeded)
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
    openai_state_recovered = recovered;
    openai_state_save_known = save_known;
    openai_state_save_succeeded = save_succeeded;
}

int openai_m133_reset_current(void)
{
    int lw, lbk, lbs, lr;
    int sw, sbk, sbs, sr;
    int valid, recovered, known, succeeded;
    int ok;

    openai_m133_save_all(
        &lw, &lbk, &lbs, &lr,
        &sw, &sbk, &sbs, &sr,
        &valid, &recovered, &known, &succeeded);

    openai_last_workflow = OPENAI_WORKFLOW_REVIEW;
    openai_last_build_known = 1;
    openai_last_build_status = 51;
    openai_last_rollback = OPENAI_ROLLBACK_SUCCEEDED;

    openai_state_reset_memory();

    ok =
        openai_last_workflow == OPENAI_WORKFLOW_NONE &&
        openai_last_build_known == 0 &&
        openai_last_build_status == 0 &&
        openai_last_rollback == OPENAI_ROLLBACK_NONE;

    openai_m133_restore_all(
        lw, lbk, lbs, lr,
        sw, sbk, sbs, sr,
        valid, recovered, known, succeeded);

    return ok;
}

int openai_m133_reset_snapshot(void)
{
    int lw, lbk, lbs, lr;
    int sw, sbk, sbs, sr;
    int valid, recovered, known, succeeded;
    int ok;

    openai_m133_save_all(
        &lw, &lbk, &lbs, &lr,
        &sw, &sbk, &sbs, &sr,
        &valid, &recovered, &known, &succeeded);

    openai_saved_workflow = OPENAI_WORKFLOW_EXECUTE;
    openai_saved_build_known = 1;
    openai_saved_build_status = 53;
    openai_saved_rollback = OPENAI_ROLLBACK_SUCCEEDED;

    openai_state_reset_memory();

    ok =
        openai_saved_workflow == OPENAI_WORKFLOW_NONE &&
        openai_saved_build_known == 0 &&
        openai_saved_build_status == 0 &&
        openai_saved_rollback == OPENAI_ROLLBACK_NONE;

    openai_m133_restore_all(
        lw, lbk, lbs, lr,
        sw, sbk, sbs, sr,
        valid, recovered, known, succeeded);

    return ok;
}

int openai_m133_reset_flags(void)
{
    int lw, lbk, lbs, lr;
    int sw, sbk, sbs, sr;
    int valid, recovered, known, succeeded;
    int ok;

    openai_m133_save_all(
        &lw, &lbk, &lbs, &lr,
        &sw, &sbk, &sbs, &sr,
        &valid, &recovered, &known, &succeeded);

    openai_state_valid = 1;
    openai_state_recovered = 1;
    openai_state_save_known = 1;
    openai_state_save_succeeded = 1;

    openai_state_reset_memory();

    ok =
        openai_state_valid == 0 &&
        openai_state_recovered == 0 &&
        openai_state_save_known == 0 &&
        openai_state_save_succeeded == 0;

    openai_m133_restore_all(
        lw, lbk, lbs, lr,
        sw, sbk, sbs, sr,
        valid, recovered, known, succeeded);

    return ok;
}

int openai_m133_load_repopulates(
    const char *path)
{
    int lw, lbk, lbs, lr;
    int sw, sbk, sbs, sr;
    int valid, recovered, known, succeeded;
    int ok;

    openai_m133_save_all(
        &lw, &lbk, &lbs, &lr,
        &sw, &sbk, &sbs, &sr,
        &valid, &recovered, &known, &succeeded);

    (void)openai_state_purge_all(path);

    if (!openai_state_save_to(
            path,
            OPENAI_WORKFLOW_EXECUTE,
            1,
            55,
            OPENAI_ROLLBACK_SUCCEEDED)) {
        openai_m133_restore_all(
            lw, lbk, lbs, lr,
            sw, sbk, sbs, sr,
            valid, recovered, known, succeeded);
        return 0;
    }

    openai_state_reset_memory();

    ok =
        openai_state_apply_load(path) &&
        openai_state_valid == 1 &&
        openai_last_workflow == OPENAI_WORKFLOW_EXECUTE &&
        openai_last_build_status == 55 &&
        openai_saved_workflow == OPENAI_WORKFLOW_EXECUTE &&
        openai_saved_build_status == 55;

    (void)openai_state_purge_all(path);

    openai_m133_restore_all(
        lw, lbk, lbs, lr,
        sw, sbk, sbs, sr,
        valid, recovered, known, succeeded);

    return ok;
}
