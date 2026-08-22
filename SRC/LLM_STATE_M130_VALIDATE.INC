static void llm_m130_save_state_flags(
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

static void llm_m130_restore_state_flags(
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

int llm_m130_first_fail_invalid(void)
{
    int loaded;
    int valid;
    int recovered;
    int save_known;
    int save_succeeded;
    int saved_workflow;
    int saved_build_known;
    int saved_build_status;
    int saved_rollback;
    int prior_valid;
    int ok;

    llm_m130_save_state_flags(
        &loaded,
        &valid,
        &recovered,
        &save_known,
        &save_succeeded,
        &saved_workflow,
        &saved_build_known,
        &saved_build_status,
        &saved_rollback);

    llm_state_loaded = 0;
    llm_state_valid = 0;
    llm_state_recovered = 0;
    llm_state_save_known = 0;
    llm_state_save_succeeded = 0;
    llm_state_snapshot_clear();

    prior_valid = llm_state_valid;
    llm_state_note_save_result(0);
    llm_state_save_known = 1;
    llm_state_save_succeeded = 0;
    llm_state_valid = prior_valid;

    ok =
        llm_state_valid == 0 &&
        llm_state_save_known == 1 &&
        llm_state_save_succeeded == 0;

    llm_m130_restore_state_flags(
        loaded,
        valid,
        recovered,
        save_known,
        save_succeeded,
        saved_workflow,
        saved_build_known,
        saved_build_status,
        saved_rollback);

    return ok;
}

int llm_m130_later_fail_valid(void)
{
    int loaded;
    int valid;
    int recovered;
    int save_known;
    int save_succeeded;
    int saved_workflow;
    int saved_build_known;
    int saved_build_status;
    int saved_rollback;
    int prior_valid;
    int ok;

    llm_m130_save_state_flags(
        &loaded,
        &valid,
        &recovered,
        &save_known,
        &save_succeeded,
        &saved_workflow,
        &saved_build_known,
        &saved_build_status,
        &saved_rollback);

    llm_state_loaded = 1;
    llm_state_valid = 1;
    llm_state_recovered = 0;
    llm_state_save_known = 1;
    llm_state_save_succeeded = 1;
    llm_saved_workflow = LLM_WORKFLOW_EXECUTE;
    llm_saved_build_known = 1;
    llm_saved_build_status = 23;
    llm_saved_rollback = LLM_ROLLBACK_SUCCEEDED;

    prior_valid = llm_state_valid;
    llm_state_note_save_result(0);
    llm_state_save_known = 1;
    llm_state_save_succeeded = 0;
    llm_state_valid = prior_valid;

    ok =
        llm_state_valid == 1 &&
        llm_saved_build_status == 23 &&
        llm_state_save_known == 1 &&
        llm_state_save_succeeded == 0;

    llm_m130_restore_state_flags(
        loaded,
        valid,
        recovered,
        save_known,
        save_succeeded,
        saved_workflow,
        saved_build_known,
        saved_build_status,
        saved_rollback);

    return ok;
}

int llm_m130_success_valid(void)
{
    int loaded;
    int valid;
    int recovered;
    int save_known;
    int save_succeeded;
    int saved_workflow;
    int saved_build_known;
    int saved_build_status;
    int saved_rollback;
    int ok;

    llm_m130_save_state_flags(
        &loaded,
        &valid,
        &recovered,
        &save_known,
        &save_succeeded,
        &saved_workflow,
        &saved_build_known,
        &saved_build_status,
        &saved_rollback);

    llm_last_workflow = LLM_WORKFLOW_EXECUTE;
    llm_last_build_known = 1;
    llm_last_build_status = 25;
    llm_last_rollback = LLM_ROLLBACK_SUCCEEDED;

    llm_state_valid = 0;
    llm_state_note_save_result(1);
    llm_state_save_known = 1;
    llm_state_save_succeeded = 1;
    llm_state_snapshot_current();

    ok =
        llm_state_valid == 1 &&
        llm_state_save_known == 1 &&
        llm_state_save_succeeded == 1 &&
        llm_saved_build_status == 25;

    llm_m130_restore_state_flags(
        loaded,
        valid,
        recovered,
        save_known,
        save_succeeded,
        saved_workflow,
        saved_build_known,
        saved_build_status,
        saved_rollback);

    return ok;
}

int llm_m130_load_resets_save(
    const char *path)
{
    int loaded;
    int valid;
    int recovered;
    int save_known;
    int save_succeeded;
    int saved_workflow;
    int saved_build_known;
    int saved_build_status;
    int saved_rollback;
    int ok;

    llm_m130_save_state_flags(
        &loaded,
        &valid,
        &recovered,
        &save_known,
        &save_succeeded,
        &saved_workflow,
        &saved_build_known,
        &saved_build_status,
        &saved_rollback);

    (void)llm_state_purge_all(path);

    if (!llm_state_save_to(
            path,
            LLM_WORKFLOW_EXECUTE,
            1,
            27,
            LLM_ROLLBACK_SUCCEEDED)) {
        llm_m130_restore_state_flags(
            loaded,
            valid,
            recovered,
            save_known,
            save_succeeded,
            saved_workflow,
            saved_build_known,
            saved_build_status,
            saved_rollback);
        return 0;
    }

    llm_state_save_known = 1;
    llm_state_save_succeeded = 0;

    ok =
        llm_state_apply_load(path) &&
        llm_state_valid == 1 &&
        llm_state_save_known == 0 &&
        llm_state_save_succeeded == 0 &&
        llm_saved_build_status == 27;

    (void)llm_state_purge_all(path);

    llm_m130_restore_state_flags(
        loaded,
        valid,
        recovered,
        save_known,
        save_succeeded,
        saved_workflow,
        saved_build_known,
        saved_build_status,
        saved_rollback);

    return ok;
}
