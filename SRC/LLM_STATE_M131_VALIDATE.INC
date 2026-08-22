static void llm_m131_save_snapshot(
    int *valid,
    int *saved_workflow,
    int *saved_build_known,
    int *saved_build_status,
    int *saved_rollback)
{
    *valid = llm_state_valid;
    *saved_workflow = llm_saved_workflow;
    *saved_build_known = llm_saved_build_known;
    *saved_build_status = llm_saved_build_status;
    *saved_rollback = llm_saved_rollback;
}

static void llm_m131_restore_snapshot(
    int valid,
    int saved_workflow,
    int saved_build_known,
    int saved_build_status,
    int saved_rollback)
{
    llm_state_valid = valid;
    llm_saved_workflow = saved_workflow;
    llm_saved_build_known = saved_build_known;
    llm_saved_build_status = saved_build_status;
    llm_saved_rollback = saved_rollback;
}

int llm_m131_clear_workflow(
    const char *path)
{
    int valid;
    int saved_workflow;
    int saved_build_known;
    int saved_build_status;
    int saved_rollback;
    int ok;

    llm_m131_save_snapshot(
        &valid,
        &saved_workflow,
        &saved_build_known,
        &saved_build_status,
        &saved_rollback);

    (void)llm_state_purge_all(path);

    llm_state_valid = 1;
    llm_saved_workflow = LLM_WORKFLOW_EXECUTE;
    llm_saved_build_known = 1;
    llm_saved_build_status = 31;
    llm_saved_rollback = LLM_ROLLBACK_SUCCEEDED;

    ok =
        !llm_state_apply_load(path) &&
        llm_state_valid == 0 &&
        llm_saved_workflow == LLM_WORKFLOW_NONE;

    llm_m131_restore_snapshot(
        valid,
        saved_workflow,
        saved_build_known,
        saved_build_status,
        saved_rollback);

    return ok;
}

int llm_m131_clear_build(
    const char *path)
{
    int valid;
    int saved_workflow;
    int saved_build_known;
    int saved_build_status;
    int saved_rollback;
    int ok;

    llm_m131_save_snapshot(
        &valid,
        &saved_workflow,
        &saved_build_known,
        &saved_build_status,
        &saved_rollback);

    (void)llm_state_purge_all(path);

    llm_state_valid = 1;
    llm_saved_workflow = LLM_WORKFLOW_EXECUTE;
    llm_saved_build_known = 1;
    llm_saved_build_status = 33;
    llm_saved_rollback = LLM_ROLLBACK_SUCCEEDED;

    ok =
        !llm_state_apply_load(path) &&
        llm_saved_build_known == 0 &&
        llm_saved_build_status == 0;

    llm_m131_restore_snapshot(
        valid,
        saved_workflow,
        saved_build_known,
        saved_build_status,
        saved_rollback);

    return ok;
}

int llm_m131_clear_rollback(
    const char *path)
{
    int valid;
    int saved_workflow;
    int saved_build_known;
    int saved_build_status;
    int saved_rollback;
    int ok;

    llm_m131_save_snapshot(
        &valid,
        &saved_workflow,
        &saved_build_known,
        &saved_build_status,
        &saved_rollback);

    (void)llm_state_purge_all(path);

    llm_state_valid = 1;
    llm_saved_workflow = LLM_WORKFLOW_EXECUTE;
    llm_saved_build_known = 1;
    llm_saved_build_status = 35;
    llm_saved_rollback = LLM_ROLLBACK_SUCCEEDED;

    ok =
        !llm_state_apply_load(path) &&
        llm_saved_rollback == LLM_ROLLBACK_NONE;

    llm_m131_restore_snapshot(
        valid,
        saved_workflow,
        saved_build_known,
        saved_build_status,
        saved_rollback);

    return ok;
}

int llm_m131_save_preserves(void)
{
    int valid;
    int saved_workflow;
    int saved_build_known;
    int saved_build_status;
    int saved_rollback;
    int prior_valid;
    int ok;

    llm_m131_save_snapshot(
        &valid,
        &saved_workflow,
        &saved_build_known,
        &saved_build_status,
        &saved_rollback);

    llm_state_valid = 1;
    llm_saved_workflow = LLM_WORKFLOW_EXECUTE;
    llm_saved_build_known = 1;
    llm_saved_build_status = 37;
    llm_saved_rollback = LLM_ROLLBACK_SUCCEEDED;

    prior_valid = llm_state_valid;
    llm_state_note_save_result(0);
    llm_state_valid = prior_valid;

    ok =
        llm_state_valid == 1 &&
        llm_saved_workflow == LLM_WORKFLOW_EXECUTE &&
        llm_saved_build_known == 1 &&
        llm_saved_build_status == 37 &&
        llm_saved_rollback == LLM_ROLLBACK_SUCCEEDED;

    llm_m131_restore_snapshot(
        valid,
        saved_workflow,
        saved_build_known,
        saved_build_status,
        saved_rollback);

    return ok;
}
