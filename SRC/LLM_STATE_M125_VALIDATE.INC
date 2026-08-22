static int llm_m125_write_bad(
    const char *path)
{
    FILE *file;
    int success;

    file = fopen(path, "w");

    if (file == NULL) {
        return 0;
    }

    success = fputs("invalid\n", file) != EOF;

    if (fclose(file) != 0) {
        success = 0;
    }

    return success;
}

static void llm_m125_save_globals(
    int *workflow,
    int *build_known,
    int *build_status,
    int *rollback,
    int *loaded,
    int *valid,
    int *recovered)
{
    *workflow = llm_last_workflow;
    *build_known = llm_last_build_known;
    *build_status = llm_last_build_status;
    *rollback = llm_last_rollback;
    *loaded = llm_state_loaded;
    *valid = llm_state_valid;
    *recovered = llm_state_recovered;
}

static void llm_m125_restore_globals(
    int workflow,
    int build_known,
    int build_status,
    int rollback,
    int loaded,
    int valid,
    int recovered)
{
    llm_last_workflow = workflow;
    llm_last_build_known = build_known;
    llm_last_build_status = build_status;
    llm_last_rollback = rollback;
    llm_state_loaded = loaded;
    llm_state_valid = valid;
    llm_state_recovered = recovered;
}

int llm_m125_fail_clears(
    const char *path)
{
    int workflow;
    int build_known;
    int build_status;
    int rollback;
    int loaded;
    int valid;
    int recovered;
    int ok;

    llm_m125_save_globals(
        &workflow,
        &build_known,
        &build_status,
        &rollback,
        &loaded,
        &valid,
        &recovered);

    (void)llm_state_purge_all(path);
    llm_state_loaded = 0;
    llm_state_valid = 1;
    llm_state_recovered = 1;

    ok =
        !llm_state_apply_load(path) &&
        llm_state_recovered == 0;

    (void)llm_state_purge_all(path);
    llm_m125_restore_globals(
        workflow,
        build_known,
        build_status,
        rollback,
        loaded,
        valid,
        recovered);

    return ok;
}

int llm_m125_fail_invalid(
    const char *path)
{
    int workflow;
    int build_known;
    int build_status;
    int rollback;
    int loaded;
    int valid;
    int recovered;
    int ok;

    llm_m125_save_globals(
        &workflow,
        &build_known,
        &build_status,
        &rollback,
        &loaded,
        &valid,
        &recovered);

    (void)llm_state_purge_all(path);
    llm_state_loaded = 0;
    llm_state_valid = 1;
    llm_state_recovered = 1;

    ok =
        !llm_state_apply_load(path) &&
        llm_state_loaded == 1 &&
        llm_state_valid == 0;

    (void)llm_state_purge_all(path);
    llm_m125_restore_globals(
        workflow,
        build_known,
        build_status,
        rollback,
        loaded,
        valid,
        recovered);

    return ok;
}

int llm_m125_normal_resets(
    const char *path)
{
    int workflow;
    int build_known;
    int build_status;
    int rollback;
    int loaded;
    int valid;
    int recovered;
    int ok;

    llm_m125_save_globals(
        &workflow,
        &build_known,
        &build_status,
        &rollback,
        &loaded,
        &valid,
        &recovered);

    (void)llm_state_purge_all(path);

    if (!llm_state_save_to(
            path,
            LLM_WORKFLOW_EXECUTE,
            1,
            7,
            LLM_ROLLBACK_SUCCEEDED)) {
        llm_m125_restore_globals(
            workflow,
            build_known,
            build_status,
            rollback,
            loaded,
            valid,
            recovered);
        return 0;
    }

    llm_state_loaded = 0;
    llm_state_valid = 1;
    llm_state_recovered = 1;

    ok =
        llm_state_apply_load(path) &&
        llm_state_valid == 1 &&
        llm_state_recovered == 0 &&
        llm_last_build_status == 7;

    (void)llm_state_purge_all(path);
    llm_m125_restore_globals(
        workflow,
        build_known,
        build_status,
        rollback,
        loaded,
        valid,
        recovered);

    return ok;
}

int llm_m125_recovery_sets(
    const char *path)
{
    int workflow;
    int build_known;
    int build_status;
    int rollback;
    int loaded;
    int valid;
    int recovered;
    int ok;

    llm_m125_save_globals(
        &workflow,
        &build_known,
        &build_status,
        &rollback,
        &loaded,
        &valid,
        &recovered);

    (void)llm_state_purge_all(path);

    if (!llm_state_save_to(
            path,
            LLM_WORKFLOW_EXECUTE,
            1,
            8,
            LLM_ROLLBACK_SUCCEEDED) ||
        !llm_state_save_to(
            path,
            LLM_WORKFLOW_EXECUTE,
            1,
            9,
            LLM_ROLLBACK_SUCCEEDED) ||
        !llm_m125_write_bad(path)) {
        (void)llm_state_purge_all(path);
        llm_m125_restore_globals(
            workflow,
            build_known,
            build_status,
            rollback,
            loaded,
            valid,
            recovered);
        return 0;
    }

    llm_state_loaded = 0;
    llm_state_valid = 0;
    llm_state_recovered = 0;

    ok =
        llm_state_apply_load(path) &&
        llm_state_valid == 1 &&
        llm_state_recovered == 1 &&
        llm_last_build_status == 9;

    (void)llm_state_purge_all(path);
    llm_m125_restore_globals(
        workflow,
        build_known,
        build_status,
        rollback,
        loaded,
        valid,
        recovered);

    return ok;
}
