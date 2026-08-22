static void openai_m131_save_snapshot(
    int *valid,
    int *saved_workflow,
    int *saved_build_known,
    int *saved_build_status,
    int *saved_rollback)
{
    *valid = openai_state_valid;
    *saved_workflow = openai_saved_workflow;
    *saved_build_known = openai_saved_build_known;
    *saved_build_status = openai_saved_build_status;
    *saved_rollback = openai_saved_rollback;
}

static void openai_m131_restore_snapshot(
    int valid,
    int saved_workflow,
    int saved_build_known,
    int saved_build_status,
    int saved_rollback)
{
    openai_state_valid = valid;
    openai_saved_workflow = saved_workflow;
    openai_saved_build_known = saved_build_known;
    openai_saved_build_status = saved_build_status;
    openai_saved_rollback = saved_rollback;
}

int openai_m131_clear_workflow(
    const char *path)
{
    int valid;
    int saved_workflow;
    int saved_build_known;
    int saved_build_status;
    int saved_rollback;
    int ok;

    openai_m131_save_snapshot(
        &valid,
        &saved_workflow,
        &saved_build_known,
        &saved_build_status,
        &saved_rollback);

    (void)openai_state_purge_all(path);

    openai_state_valid = 1;
    openai_saved_workflow = OPENAI_WORKFLOW_EXECUTE;
    openai_saved_build_known = 1;
    openai_saved_build_status = 31;
    openai_saved_rollback = OPENAI_ROLLBACK_SUCCEEDED;

    ok =
        !openai_state_apply_load(path) &&
        openai_state_valid == 0 &&
        openai_saved_workflow == OPENAI_WORKFLOW_NONE;

    openai_m131_restore_snapshot(
        valid,
        saved_workflow,
        saved_build_known,
        saved_build_status,
        saved_rollback);

    return ok;
}

int openai_m131_clear_build(
    const char *path)
{
    int valid;
    int saved_workflow;
    int saved_build_known;
    int saved_build_status;
    int saved_rollback;
    int ok;

    openai_m131_save_snapshot(
        &valid,
        &saved_workflow,
        &saved_build_known,
        &saved_build_status,
        &saved_rollback);

    (void)openai_state_purge_all(path);

    openai_state_valid = 1;
    openai_saved_workflow = OPENAI_WORKFLOW_EXECUTE;
    openai_saved_build_known = 1;
    openai_saved_build_status = 33;
    openai_saved_rollback = OPENAI_ROLLBACK_SUCCEEDED;

    ok =
        !openai_state_apply_load(path) &&
        openai_saved_build_known == 0 &&
        openai_saved_build_status == 0;

    openai_m131_restore_snapshot(
        valid,
        saved_workflow,
        saved_build_known,
        saved_build_status,
        saved_rollback);

    return ok;
}

int openai_m131_clear_rollback(
    const char *path)
{
    int valid;
    int saved_workflow;
    int saved_build_known;
    int saved_build_status;
    int saved_rollback;
    int ok;

    openai_m131_save_snapshot(
        &valid,
        &saved_workflow,
        &saved_build_known,
        &saved_build_status,
        &saved_rollback);

    (void)openai_state_purge_all(path);

    openai_state_valid = 1;
    openai_saved_workflow = OPENAI_WORKFLOW_EXECUTE;
    openai_saved_build_known = 1;
    openai_saved_build_status = 35;
    openai_saved_rollback = OPENAI_ROLLBACK_SUCCEEDED;

    ok =
        !openai_state_apply_load(path) &&
        openai_saved_rollback == OPENAI_ROLLBACK_NONE;

    openai_m131_restore_snapshot(
        valid,
        saved_workflow,
        saved_build_known,
        saved_build_status,
        saved_rollback);

    return ok;
}

int openai_m131_save_preserves(void)
{
    int valid;
    int saved_workflow;
    int saved_build_known;
    int saved_build_status;
    int saved_rollback;
    int prior_valid;
    int ok;

    openai_m131_save_snapshot(
        &valid,
        &saved_workflow,
        &saved_build_known,
        &saved_build_status,
        &saved_rollback);

    openai_state_valid = 1;
    openai_saved_workflow = OPENAI_WORKFLOW_EXECUTE;
    openai_saved_build_known = 1;
    openai_saved_build_status = 37;
    openai_saved_rollback = OPENAI_ROLLBACK_SUCCEEDED;

    prior_valid = openai_state_valid;
    openai_state_note_save_result(0);
    openai_state_valid = prior_valid;

    ok =
        openai_state_valid == 1 &&
        openai_saved_workflow == OPENAI_WORKFLOW_EXECUTE &&
        openai_saved_build_known == 1 &&
        openai_saved_build_status == 37 &&
        openai_saved_rollback == OPENAI_ROLLBACK_SUCCEEDED;

    openai_m131_restore_snapshot(
        valid,
        saved_workflow,
        saved_build_known,
        saved_build_status,
        saved_rollback);

    return ok;
}
