static void llm_m133_save_all(
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
    *last_workflow = llm_last_workflow;
    *last_build_known = llm_last_build_known;
    *last_build_status = llm_last_build_status;
    *last_rollback = llm_last_rollback;
    *saved_workflow = llm_saved_workflow;
    *saved_build_known = llm_saved_build_known;
    *saved_build_status = llm_saved_build_status;
    *saved_rollback = llm_saved_rollback;
    *valid = llm_state_valid;
    *recovered = llm_state_recovered;
    *save_known = llm_state_save_known;
    *save_succeeded = llm_state_save_succeeded;
}

static void llm_m133_restore_all(
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
    llm_last_workflow = last_workflow;
    llm_last_build_known = last_build_known;
    llm_last_build_status = last_build_status;
    llm_last_rollback = last_rollback;
    llm_saved_workflow = saved_workflow;
    llm_saved_build_known = saved_build_known;
    llm_saved_build_status = saved_build_status;
    llm_saved_rollback = saved_rollback;
    llm_state_valid = valid;
    llm_state_recovered = recovered;
    llm_state_save_known = save_known;
    llm_state_save_succeeded = save_succeeded;
}

int llm_m133_reset_current(void)
{
    int lw, lbk, lbs, lr;
    int sw, sbk, sbs, sr;
    int valid, recovered, known, succeeded;
    int ok;

    llm_m133_save_all(
        &lw, &lbk, &lbs, &lr,
        &sw, &sbk, &sbs, &sr,
        &valid, &recovered, &known, &succeeded);

    llm_last_workflow = LLM_WORKFLOW_REVIEW;
    llm_last_build_known = 1;
    llm_last_build_status = 51;
    llm_last_rollback = LLM_ROLLBACK_SUCCEEDED;

    llm_state_reset_memory();

    ok =
        llm_last_workflow == LLM_WORKFLOW_NONE &&
        llm_last_build_known == 0 &&
        llm_last_build_status == 0 &&
        llm_last_rollback == LLM_ROLLBACK_NONE;

    llm_m133_restore_all(
        lw, lbk, lbs, lr,
        sw, sbk, sbs, sr,
        valid, recovered, known, succeeded);

    return ok;
}

int llm_m133_reset_snapshot(void)
{
    int lw, lbk, lbs, lr;
    int sw, sbk, sbs, sr;
    int valid, recovered, known, succeeded;
    int ok;

    llm_m133_save_all(
        &lw, &lbk, &lbs, &lr,
        &sw, &sbk, &sbs, &sr,
        &valid, &recovered, &known, &succeeded);

    llm_saved_workflow = LLM_WORKFLOW_EXECUTE;
    llm_saved_build_known = 1;
    llm_saved_build_status = 53;
    llm_saved_rollback = LLM_ROLLBACK_SUCCEEDED;

    llm_state_reset_memory();

    ok =
        llm_saved_workflow == LLM_WORKFLOW_NONE &&
        llm_saved_build_known == 0 &&
        llm_saved_build_status == 0 &&
        llm_saved_rollback == LLM_ROLLBACK_NONE;

    llm_m133_restore_all(
        lw, lbk, lbs, lr,
        sw, sbk, sbs, sr,
        valid, recovered, known, succeeded);

    return ok;
}

int llm_m133_reset_flags(void)
{
    int lw, lbk, lbs, lr;
    int sw, sbk, sbs, sr;
    int valid, recovered, known, succeeded;
    int ok;

    llm_m133_save_all(
        &lw, &lbk, &lbs, &lr,
        &sw, &sbk, &sbs, &sr,
        &valid, &recovered, &known, &succeeded);

    llm_state_valid = 1;
    llm_state_recovered = 1;
    llm_state_save_known = 1;
    llm_state_save_succeeded = 1;

    llm_state_reset_memory();

    ok =
        llm_state_valid == 0 &&
        llm_state_recovered == 0 &&
        llm_state_save_known == 0 &&
        llm_state_save_succeeded == 0;

    llm_m133_restore_all(
        lw, lbk, lbs, lr,
        sw, sbk, sbs, sr,
        valid, recovered, known, succeeded);

    return ok;
}

int llm_m133_load_repopulates(
    const char *path)
{
    int lw, lbk, lbs, lr;
    int sw, sbk, sbs, sr;
    int valid, recovered, known, succeeded;
    int ok;

    llm_m133_save_all(
        &lw, &lbk, &lbs, &lr,
        &sw, &sbk, &sbs, &sr,
        &valid, &recovered, &known, &succeeded);

    (void)llm_state_purge_all(path);

    if (!llm_state_save_to(
            path,
            LLM_WORKFLOW_EXECUTE,
            1,
            55,
            LLM_ROLLBACK_SUCCEEDED)) {
        llm_m133_restore_all(
            lw, lbk, lbs, lr,
            sw, sbk, sbs, sr,
            valid, recovered, known, succeeded);
        return 0;
    }

    llm_state_reset_memory();

    ok =
        llm_state_apply_load(path) &&
        llm_state_valid == 1 &&
        llm_last_workflow == LLM_WORKFLOW_EXECUTE &&
        llm_last_build_status == 55 &&
        llm_saved_workflow == LLM_WORKFLOW_EXECUTE &&
        llm_saved_build_status == 55;

    (void)llm_state_purge_all(path);

    llm_m133_restore_all(
        lw, lbk, lbs, lr,
        sw, sbk, sbs, sr,
        valid, recovered, known, succeeded);

    return ok;
}
