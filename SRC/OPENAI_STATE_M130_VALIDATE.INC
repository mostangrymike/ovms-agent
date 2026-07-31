static void openai_m130_save_state_flags(
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

static void openai_m130_restore_state_flags(
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

int openai_m130_first_fail_invalid(void)
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

    openai_m130_save_state_flags(
        &loaded,
        &valid,
        &recovered,
        &save_known,
        &save_succeeded,
        &saved_workflow,
        &saved_build_known,
        &saved_build_status,
        &saved_rollback);

    openai_state_loaded = 0;
    openai_state_valid = 0;
    openai_state_recovered = 0;
    openai_state_save_known = 0;
    openai_state_save_succeeded = 0;
    openai_state_snapshot_clear();

    prior_valid = openai_state_valid;
    openai_state_note_save_result(0);
    openai_state_save_known = 1;
    openai_state_save_succeeded = 0;
    openai_state_valid = prior_valid;

    ok =
        openai_state_valid == 0 &&
        openai_state_save_known == 1 &&
        openai_state_save_succeeded == 0;

    openai_m130_restore_state_flags(
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

int openai_m130_later_fail_valid(void)
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

    openai_m130_save_state_flags(
        &loaded,
        &valid,
        &recovered,
        &save_known,
        &save_succeeded,
        &saved_workflow,
        &saved_build_known,
        &saved_build_status,
        &saved_rollback);

    openai_state_loaded = 1;
    openai_state_valid = 1;
    openai_state_recovered = 0;
    openai_state_save_known = 1;
    openai_state_save_succeeded = 1;
    openai_saved_workflow = OPENAI_WORKFLOW_EXECUTE;
    openai_saved_build_known = 1;
    openai_saved_build_status = 23;
    openai_saved_rollback = OPENAI_ROLLBACK_SUCCEEDED;

    prior_valid = openai_state_valid;
    openai_state_note_save_result(0);
    openai_state_save_known = 1;
    openai_state_save_succeeded = 0;
    openai_state_valid = prior_valid;

    ok =
        openai_state_valid == 1 &&
        openai_saved_build_status == 23 &&
        openai_state_save_known == 1 &&
        openai_state_save_succeeded == 0;

    openai_m130_restore_state_flags(
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

int openai_m130_success_valid(void)
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

    openai_m130_save_state_flags(
        &loaded,
        &valid,
        &recovered,
        &save_known,
        &save_succeeded,
        &saved_workflow,
        &saved_build_known,
        &saved_build_status,
        &saved_rollback);

    openai_last_workflow = OPENAI_WORKFLOW_EXECUTE;
    openai_last_build_known = 1;
    openai_last_build_status = 25;
    openai_last_rollback = OPENAI_ROLLBACK_SUCCEEDED;

    openai_state_valid = 0;
    openai_state_note_save_result(1);
    openai_state_save_known = 1;
    openai_state_save_succeeded = 1;
    openai_state_snapshot_current();

    ok =
        openai_state_valid == 1 &&
        openai_state_save_known == 1 &&
        openai_state_save_succeeded == 1 &&
        openai_saved_build_status == 25;

    openai_m130_restore_state_flags(
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

int openai_m130_load_resets_save(
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

    openai_m130_save_state_flags(
        &loaded,
        &valid,
        &recovered,
        &save_known,
        &save_succeeded,
        &saved_workflow,
        &saved_build_known,
        &saved_build_status,
        &saved_rollback);

    (void)openai_state_purge_all(path);

    if (!openai_state_save_to(
            path,
            OPENAI_WORKFLOW_EXECUTE,
            1,
            27,
            OPENAI_ROLLBACK_SUCCEEDED)) {
        openai_m130_restore_state_flags(
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

    openai_state_save_known = 1;
    openai_state_save_succeeded = 0;

    ok =
        openai_state_apply_load(path) &&
        openai_state_valid == 1 &&
        openai_state_save_known == 0 &&
        openai_state_save_succeeded == 0 &&
        openai_saved_build_status == 27;

    (void)openai_state_purge_all(path);

    openai_m130_restore_state_flags(
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
