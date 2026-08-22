static int openai_m125_write_bad(
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

static void openai_m125_save_globals(
    int *workflow,
    int *build_known,
    int *build_status,
    int *rollback,
    int *loaded,
    int *valid,
    int *recovered)
{
    *workflow = openai_last_workflow;
    *build_known = openai_last_build_known;
    *build_status = openai_last_build_status;
    *rollback = openai_last_rollback;
    *loaded = openai_state_loaded;
    *valid = openai_state_valid;
    *recovered = openai_state_recovered;
}

static void openai_m125_restore_globals(
    int workflow,
    int build_known,
    int build_status,
    int rollback,
    int loaded,
    int valid,
    int recovered)
{
    openai_last_workflow = workflow;
    openai_last_build_known = build_known;
    openai_last_build_status = build_status;
    openai_last_rollback = rollback;
    openai_state_loaded = loaded;
    openai_state_valid = valid;
    openai_state_recovered = recovered;
}

int openai_m125_fail_clears(
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

    openai_m125_save_globals(
        &workflow,
        &build_known,
        &build_status,
        &rollback,
        &loaded,
        &valid,
        &recovered);

    (void)openai_state_purge_all(path);
    openai_state_loaded = 0;
    openai_state_valid = 1;
    openai_state_recovered = 1;

    ok =
        !openai_state_apply_load(path) &&
        openai_state_recovered == 0;

    (void)openai_state_purge_all(path);
    openai_m125_restore_globals(
        workflow,
        build_known,
        build_status,
        rollback,
        loaded,
        valid,
        recovered);

    return ok;
}

int openai_m125_fail_invalid(
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

    openai_m125_save_globals(
        &workflow,
        &build_known,
        &build_status,
        &rollback,
        &loaded,
        &valid,
        &recovered);

    (void)openai_state_purge_all(path);
    openai_state_loaded = 0;
    openai_state_valid = 1;
    openai_state_recovered = 1;

    ok =
        !openai_state_apply_load(path) &&
        openai_state_loaded == 1 &&
        openai_state_valid == 0;

    (void)openai_state_purge_all(path);
    openai_m125_restore_globals(
        workflow,
        build_known,
        build_status,
        rollback,
        loaded,
        valid,
        recovered);

    return ok;
}

int openai_m125_normal_resets(
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

    openai_m125_save_globals(
        &workflow,
        &build_known,
        &build_status,
        &rollback,
        &loaded,
        &valid,
        &recovered);

    (void)openai_state_purge_all(path);

    if (!openai_state_save_to(
            path,
            OPENAI_WORKFLOW_EXECUTE,
            1,
            7,
            OPENAI_ROLLBACK_SUCCEEDED)) {
        openai_m125_restore_globals(
            workflow,
            build_known,
            build_status,
            rollback,
            loaded,
            valid,
            recovered);
        return 0;
    }

    openai_state_loaded = 0;
    openai_state_valid = 1;
    openai_state_recovered = 1;

    ok =
        openai_state_apply_load(path) &&
        openai_state_valid == 1 &&
        openai_state_recovered == 0 &&
        openai_last_build_status == 7;

    (void)openai_state_purge_all(path);
    openai_m125_restore_globals(
        workflow,
        build_known,
        build_status,
        rollback,
        loaded,
        valid,
        recovered);

    return ok;
}

int openai_m125_recovery_sets(
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

    openai_m125_save_globals(
        &workflow,
        &build_known,
        &build_status,
        &rollback,
        &loaded,
        &valid,
        &recovered);

    (void)openai_state_purge_all(path);

    if (!openai_state_save_to(
            path,
            OPENAI_WORKFLOW_EXECUTE,
            1,
            8,
            OPENAI_ROLLBACK_SUCCEEDED) ||
        !openai_state_save_to(
            path,
            OPENAI_WORKFLOW_EXECUTE,
            1,
            9,
            OPENAI_ROLLBACK_SUCCEEDED) ||
        !openai_m125_write_bad(path)) {
        (void)openai_state_purge_all(path);
        openai_m125_restore_globals(
            workflow,
            build_known,
            build_status,
            rollback,
            loaded,
            valid,
            recovered);
        return 0;
    }

    openai_state_loaded = 0;
    openai_state_valid = 0;
    openai_state_recovered = 0;

    ok =
        openai_state_apply_load(path) &&
        openai_state_valid == 1 &&
        openai_state_recovered == 1 &&
        openai_last_build_status == 9;

    (void)openai_state_purge_all(path);
    openai_m125_restore_globals(
        workflow,
        build_known,
        build_status,
        rollback,
        loaded,
        valid,
        recovered);

    return ok;
}
