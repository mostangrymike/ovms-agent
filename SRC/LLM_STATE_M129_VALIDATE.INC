static void llm_m129_save_flags(
    int *loaded,
    int *valid,
    int *recovered,
    int *save_known,
    int *save_succeeded,
    int *saved_workflow,
    int *saved_build_known,
    int *saved_build_status,
    int *saved_rollback)
{
    *loaded = llm_state_loaded;
    *valid = llm_state_valid;
    *recovered = llm_state_recovered;
    *save_known = llm_state_save_known;
    *save_succeeded = llm_state_save_succeeded;
    *saved_workflow = llm_saved_workflow;
    *saved_build_known = llm_saved_build_known;
    *saved_build_status = llm_saved_build_status;
    *saved_rollback = llm_saved_rollback;
}

static void llm_m129_restore_flags(
    int loaded,
    int valid,
    int recovered,
    int save_known,
    int save_succeeded,
    int saved_workflow,
    int saved_build_known,
    int saved_build_status,
    int saved_rollback)
{
    llm_state_loaded = loaded;
    llm_state_valid = valid;
    llm_state_recovered = recovered;
    llm_state_save_known = save_known;
    llm_state_save_succeeded = save_succeeded;
    llm_saved_workflow = saved_workflow;
    llm_saved_build_known = saved_build_known;
    llm_saved_build_status = saved_build_status;
    llm_saved_rollback = saved_rollback;
}

int llm_m129_fail_keeps_valid(void)
{
    int loaded, valid, recovered;
    int save_known, save_succeeded;
    int sw, sbk, sbs, sr;
    int prior_valid;
    int ok;

    llm_m129_save_flags(
        &loaded, &valid, &recovered,
        &save_known, &save_succeeded,
        &sw, &sbk, &sbs, &sr);

    llm_state_loaded = 1;
    llm_state_valid = 1;
    llm_state_recovered = 0;
    llm_state_save_known = 0;
    llm_state_save_succeeded = 0;

    prior_valid = llm_state_valid;
    llm_state_note_save_result(0);
    llm_state_save_known = 1;
    llm_state_save_succeeded = 0;
    llm_state_valid = prior_valid;

    ok =
        llm_state_valid == 1 &&
        llm_state_save_known == 1 &&
        llm_state_save_succeeded == 0;

    llm_m129_restore_flags(
        loaded, valid, recovered,
        save_known, save_succeeded,
        sw, sbk, sbs, sr);

    return ok;
}

int llm_m129_fail_keeps_values(void)
{
    int loaded, valid, recovered;
    int save_known, save_succeeded;
    int sw, sbk, sbs, sr;
    int prior_valid;
    int ok;

    llm_m129_save_flags(
        &loaded, &valid, &recovered,
        &save_known, &save_succeeded,
        &sw, &sbk, &sbs, &sr);

    llm_saved_workflow = LLM_WORKFLOW_EXECUTE;
    llm_saved_build_known = 1;
    llm_saved_build_status = 21;
    llm_saved_rollback = LLM_ROLLBACK_SUCCEEDED;
    llm_state_valid = 1;

    prior_valid = llm_state_valid;
    llm_state_note_save_result(0);
    llm_state_save_known = 1;
    llm_state_save_succeeded = 0;
    llm_state_valid = prior_valid;

    ok =
        llm_saved_workflow == LLM_WORKFLOW_EXECUTE &&
        llm_saved_build_known == 1 &&
        llm_saved_build_status == 21 &&
        llm_saved_rollback == LLM_ROLLBACK_SUCCEEDED;

    llm_m129_restore_flags(
        loaded, valid, recovered,
        save_known, save_succeeded,
        sw, sbk, sbs, sr);

    return ok;
}

int llm_m129_failure_reported(void)
{
    return strcmp(
        llm_state_save_name(1, 0),
        "failure") == 0;
}

int llm_m129_success_clears(void)
{
    return
        strcmp(
            llm_state_save_name(1, 1),
            "success") == 0 &&
        strcmp(
            llm_state_save_name(0, 0),
            "not attempted") == 0;
}
