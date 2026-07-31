static void openai_m134_save_state(
    int *last_workflow,
    int *last_build_known,
    int *last_build_status,
    int *last_rollback,
    int *saved_workflow,
    int *saved_build_known,
    int *saved_build_status,
    int *saved_rollback,
    int *loaded,
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
    *loaded = openai_state_loaded;
    *valid = openai_state_valid;
    *recovered = openai_state_recovered;
    *save_known = openai_state_save_known;
    *save_succeeded = openai_state_save_succeeded;
}

static void openai_m134_restore(
    int last_workflow,
    int last_build_known,
    int last_build_status,
    int last_rollback,
    int saved_workflow,
    int saved_build_known,
    int saved_build_status,
    int saved_rollback,
    int loaded,
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
    openai_state_loaded = loaded;
    openai_state_valid = valid;
    openai_state_recovered = recovered;
    openai_state_save_known = save_known;
    openai_state_save_succeeded = save_succeeded;
}

int openai_m134_reset_consolidated(void)
{
    int lw, lbk, lbs, lr;
    int sw, sbk, sbs, sr;
    int loaded, valid, recovered, known, succeeded;
    int ok;

    openai_m134_save_state(
        &lw, &lbk, &lbs, &lr,
        &sw, &sbk, &sbs, &sr,
        &loaded, &valid, &recovered,
        &known, &succeeded);

    openai_last_workflow = OPENAI_WORKFLOW_REVIEW;
    openai_last_build_known = 1;
    openai_last_build_status = 61;
    openai_last_rollback = OPENAI_ROLLBACK_SUCCEEDED;
    openai_saved_workflow = OPENAI_WORKFLOW_EXECUTE;
    openai_saved_build_known = 1;
    openai_saved_build_status = 63;
    openai_saved_rollback = OPENAI_ROLLBACK_SUCCEEDED;
    openai_state_valid = 1;
    openai_state_recovered = 1;
    openai_state_save_known = 1;
    openai_state_save_succeeded = 1;

    openai_state_reset_memory();

    ok =
        openai_last_workflow == OPENAI_WORKFLOW_NONE &&
        openai_last_build_known == 0 &&
        openai_last_build_status == 0 &&
        openai_last_rollback == OPENAI_ROLLBACK_NONE &&
        openai_saved_workflow == OPENAI_WORKFLOW_NONE &&
        openai_saved_build_known == 0 &&
        openai_saved_build_status == 0 &&
        openai_saved_rollback == OPENAI_ROLLBACK_NONE &&
        openai_state_valid == 0 &&
        openai_state_recovered == 0 &&
        openai_state_save_known == 0 &&
        openai_state_save_succeeded == 0 &&
        openai_state_loaded == loaded;

    openai_m134_restore(
        lw, lbk, lbs, lr,
        sw, sbk, sbs, sr,
        loaded, valid, recovered,
        known, succeeded);

    return ok;
}

int openai_m134_models_distinct(void)
{
    int lw, lbk, lbs, lr;
    int sw, sbk, sbs, sr;
    int loaded, valid, recovered, known, succeeded;
    int ok;

    openai_m134_save_state(
        &lw, &lbk, &lbs, &lr,
        &sw, &sbk, &sbs, &sr,
        &loaded, &valid, &recovered,
        &known, &succeeded);

    openai_last_workflow = OPENAI_WORKFLOW_REVIEW;
    openai_last_build_known = 1;
    openai_last_build_status = 65;
    openai_last_rollback = OPENAI_ROLLBACK_NONE;

    openai_saved_workflow = OPENAI_WORKFLOW_EXECUTE;
    openai_saved_build_known = 1;
    openai_saved_build_status = 67;
    openai_saved_rollback = OPENAI_ROLLBACK_SUCCEEDED;

    ok =
        openai_last_workflow != openai_saved_workflow &&
        openai_last_build_status != openai_saved_build_status &&
        openai_last_rollback != openai_saved_rollback;

    openai_m134_restore(
        lw, lbk, lbs, lr,
        sw, sbk, sbs, sr,
        loaded, valid, recovered,
        known, succeeded);

    return ok;
}

int openai_m134_invalid_suppressed(void)
{
    return
        strcmp(
            openai_persist_workflow(
                0,
                OPENAI_WORKFLOW_EXECUTE),
            "unavailable") == 0 &&
        strcmp(
            openai_persist_build(
                0,
                1,
                69),
            "unavailable") == 0 &&
        strcmp(
            openai_persist_rollback(
                0,
                OPENAI_ROLLBACK_SUCCEEDED),
            "unavailable") == 0 &&
        strcmp(
            openai_state_source_name(
                0,
                1),
            "unavailable") == 0;
}

int openai_m134_final_audit(void)
{
    return
        strcmp(
            openai_state_save_name(0, 0),
            "not attempted") == 0 &&
        strcmp(
            openai_state_save_name(1, 0),
            "failure") == 0 &&
        strcmp(
            openai_state_save_name(1, 1),
            "success") == 0 &&
        strcmp(
            openai_state_source_name(1, 0),
            "newest version") == 0 &&
        strcmp(
            openai_state_source_name(1, 1),
            "previous version recovery") == 0;
}
