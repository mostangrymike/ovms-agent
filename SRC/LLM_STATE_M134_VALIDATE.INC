static void llm_m134_save_state(
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
    *last_workflow = llm_last_workflow;
    *last_build_known = llm_last_build_known;
    *last_build_status = llm_last_build_status;
    *last_rollback = llm_last_rollback;
    *saved_workflow = llm_saved_workflow;
    *saved_build_known = llm_saved_build_known;
    *saved_build_status = llm_saved_build_status;
    *saved_rollback = llm_saved_rollback;
    *loaded = llm_state_loaded;
    *valid = llm_state_valid;
    *recovered = llm_state_recovered;
    *save_known = llm_state_save_known;
    *save_succeeded = llm_state_save_succeeded;
}

static void llm_m134_restore(
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
    llm_last_workflow = last_workflow;
    llm_last_build_known = last_build_known;
    llm_last_build_status = last_build_status;
    llm_last_rollback = last_rollback;
    llm_saved_workflow = saved_workflow;
    llm_saved_build_known = saved_build_known;
    llm_saved_build_status = saved_build_status;
    llm_saved_rollback = saved_rollback;
    llm_state_loaded = loaded;
    llm_state_valid = valid;
    llm_state_recovered = recovered;
    llm_state_save_known = save_known;
    llm_state_save_succeeded = save_succeeded;
}

int llm_m134_reset_consolidated(void)
{
    int lw, lbk, lbs, lr;
    int sw, sbk, sbs, sr;
    int loaded, valid, recovered, known, succeeded;
    int ok;

    llm_m134_save_state(
        &lw, &lbk, &lbs, &lr,
        &sw, &sbk, &sbs, &sr,
        &loaded, &valid, &recovered,
        &known, &succeeded);

    llm_last_workflow = LLM_WORKFLOW_REVIEW;
    llm_last_build_known = 1;
    llm_last_build_status = 61;
    llm_last_rollback = LLM_ROLLBACK_SUCCEEDED;
    llm_saved_workflow = LLM_WORKFLOW_EXECUTE;
    llm_saved_build_known = 1;
    llm_saved_build_status = 63;
    llm_saved_rollback = LLM_ROLLBACK_SUCCEEDED;
    llm_state_valid = 1;
    llm_state_recovered = 1;
    llm_state_save_known = 1;
    llm_state_save_succeeded = 1;

    llm_state_reset_memory();

    ok =
        llm_last_workflow == LLM_WORKFLOW_NONE &&
        llm_last_build_known == 0 &&
        llm_last_build_status == 0 &&
        llm_last_rollback == LLM_ROLLBACK_NONE &&
        llm_saved_workflow == LLM_WORKFLOW_NONE &&
        llm_saved_build_known == 0 &&
        llm_saved_build_status == 0 &&
        llm_saved_rollback == LLM_ROLLBACK_NONE &&
        llm_state_valid == 0 &&
        llm_state_recovered == 0 &&
        llm_state_save_known == 0 &&
        llm_state_save_succeeded == 0 &&
        llm_state_loaded == loaded;

    llm_m134_restore(
        lw, lbk, lbs, lr,
        sw, sbk, sbs, sr,
        loaded, valid, recovered,
        known, succeeded);

    return ok;
}

int llm_m134_models_distinct(void)
{
    int lw, lbk, lbs, lr;
    int sw, sbk, sbs, sr;
    int loaded, valid, recovered, known, succeeded;
    int ok;

    llm_m134_save_state(
        &lw, &lbk, &lbs, &lr,
        &sw, &sbk, &sbs, &sr,
        &loaded, &valid, &recovered,
        &known, &succeeded);

    llm_last_workflow = LLM_WORKFLOW_REVIEW;
    llm_last_build_known = 1;
    llm_last_build_status = 65;
    llm_last_rollback = LLM_ROLLBACK_NONE;

    llm_saved_workflow = LLM_WORKFLOW_EXECUTE;
    llm_saved_build_known = 1;
    llm_saved_build_status = 67;
    llm_saved_rollback = LLM_ROLLBACK_SUCCEEDED;

    ok =
        llm_last_workflow != llm_saved_workflow &&
        llm_last_build_status != llm_saved_build_status &&
        llm_last_rollback != llm_saved_rollback;

    llm_m134_restore(
        lw, lbk, lbs, lr,
        sw, sbk, sbs, sr,
        loaded, valid, recovered,
        known, succeeded);

    return ok;
}

int llm_m134_invalid_suppressed(void)
{
    return
        strcmp(
            llm_persist_workflow(
                0,
                LLM_WORKFLOW_EXECUTE),
            "unavailable") == 0 &&
        strcmp(
            llm_persist_build(
                0,
                1,
                69),
            "unavailable") == 0 &&
        strcmp(
            llm_persist_rollback(
                0,
                LLM_ROLLBACK_SUCCEEDED),
            "unavailable") == 0 &&
        strcmp(
            llm_state_source_name(
                0,
                1),
            "unavailable") == 0;
}

int llm_m134_final_audit(void)
{
    return
        strcmp(
            llm_state_save_name(0, 0),
            "not attempted") == 0 &&
        strcmp(
            llm_state_save_name(1, 0),
            "failure") == 0 &&
        strcmp(
            llm_state_save_name(1, 1),
            "success") == 0 &&
        strcmp(
            llm_state_source_name(1, 0),
            "newest version") == 0 &&
        strcmp(
            llm_state_source_name(1, 1),
            "previous version recovery") == 0;
}
