static void openai_m129_save_flags(
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
    *loaded = openai_state_loaded;
    *valid = openai_state_valid;
    *recovered = openai_state_recovered;
    *save_known = openai_state_save_known;
    *save_succeeded = openai_state_save_succeeded;
    *saved_workflow = openai_saved_workflow;
    *saved_build_known = openai_saved_build_known;
    *saved_build_status = openai_saved_build_status;
    *saved_rollback = openai_saved_rollback;
}

static void openai_m129_restore_flags(
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
    openai_state_loaded = loaded;
    openai_state_valid = valid;
    openai_state_recovered = recovered;
    openai_state_save_known = save_known;
    openai_state_save_succeeded = save_succeeded;
    openai_saved_workflow = saved_workflow;
    openai_saved_build_known = saved_build_known;
    openai_saved_build_status = saved_build_status;
    openai_saved_rollback = saved_rollback;
}

int openai_m129_fail_keeps_valid(void)
{
    int loaded, valid, recovered;
    int save_known, save_succeeded;
    int sw, sbk, sbs, sr;
    int prior_valid;
    int ok;

    openai_m129_save_flags(
        &loaded, &valid, &recovered,
        &save_known, &save_succeeded,
        &sw, &sbk, &sbs, &sr);

    openai_state_loaded = 1;
    openai_state_valid = 1;
    openai_state_recovered = 0;
    openai_state_save_known = 0;
    openai_state_save_succeeded = 0;

    prior_valid = openai_state_valid;
    openai_state_note_save_result(0);
    openai_state_save_known = 1;
    openai_state_save_succeeded = 0;
    openai_state_valid = prior_valid;

    ok =
        openai_state_valid == 1 &&
        openai_state_save_known == 1 &&
        openai_state_save_succeeded == 0;

    openai_m129_restore_flags(
        loaded, valid, recovered,
        save_known, save_succeeded,
        sw, sbk, sbs, sr);

    return ok;
}

int openai_m129_fail_keeps_values(void)
{
    int loaded, valid, recovered;
    int save_known, save_succeeded;
    int sw, sbk, sbs, sr;
    int prior_valid;
    int ok;

    openai_m129_save_flags(
        &loaded, &valid, &recovered,
        &save_known, &save_succeeded,
        &sw, &sbk, &sbs, &sr);

    openai_saved_workflow = OPENAI_WORKFLOW_EXECUTE;
    openai_saved_build_known = 1;
    openai_saved_build_status = 21;
    openai_saved_rollback = OPENAI_ROLLBACK_SUCCEEDED;
    openai_state_valid = 1;

    prior_valid = openai_state_valid;
    openai_state_note_save_result(0);
    openai_state_save_known = 1;
    openai_state_save_succeeded = 0;
    openai_state_valid = prior_valid;

    ok =
        openai_saved_workflow == OPENAI_WORKFLOW_EXECUTE &&
        openai_saved_build_known == 1 &&
        openai_saved_build_status == 21 &&
        openai_saved_rollback == OPENAI_ROLLBACK_SUCCEEDED;

    openai_m129_restore_flags(
        loaded, valid, recovered,
        save_known, save_succeeded,
        sw, sbk, sbs, sr);

    return ok;
}

int openai_m129_failure_reported(void)
{
    return strcmp(
        openai_state_save_name(1, 0),
        "failure") == 0;
}

int openai_m129_success_clears(void)
{
    return
        strcmp(
            openai_state_save_name(1, 1),
            "success") == 0 &&
        strcmp(
            openai_state_save_name(0, 0),
            "not attempted") == 0;
}
