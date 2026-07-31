static int openai_m123_write_invalid(
    const char *path)
{
    FILE *file;
    int success;

    file = fopen(path, "w");

    if (file == NULL) {
        return 0;
    }

    success =
        fputs(
            "format=1\n"
            "last_workflow=invalid\n",
            file) != EOF;

    if (fclose(file) != 0) {
        success = 0;
    }

    return success;
}

static int openai_m123_load_status(
    const char *path,
    int *status_out)
{
    int workflow;
    int build_known;
    int build_status;
    int rollback;

    if (status_out == NULL) {
        return 0;
    }

    if (!openai_state_load_recovery(
            path,
            &workflow,
            &build_known,
            &build_status,
            &rollback)) {
        return 0;
    }

    if (workflow != OPENAI_WORKFLOW_EXECUTE ||
        build_known != 1 ||
        rollback != OPENAI_ROLLBACK_SUCCEEDED) {
        return 0;
    }

    *status_out = build_status;
    return 1;
}

int openai_m123_newest_preferred(
    const char *path)
{
    int status;

    (void)openai_state_purge_all(path);

    if (!openai_state_save_to(
            path,
            OPENAI_WORKFLOW_EXECUTE,
            1,
            1,
            OPENAI_ROLLBACK_SUCCEEDED) ||
        !openai_state_save_to(
            path,
            OPENAI_WORKFLOW_EXECUTE,
            1,
            2,
            OPENAI_ROLLBACK_SUCCEEDED) ||
        !openai_m123_load_status(
            path,
            &status)) {
        (void)openai_state_purge_all(path);
        return 0;
    }

    (void)openai_state_purge_all(path);
    return status == 2;
}

int openai_m123_fallback_prior(
    const char *path)
{
    int status;

    (void)openai_state_purge_all(path);

    if (!openai_state_save_to(
            path,
            OPENAI_WORKFLOW_EXECUTE,
            1,
            1,
            OPENAI_ROLLBACK_SUCCEEDED) ||
        !openai_state_save_to(
            path,
            OPENAI_WORKFLOW_EXECUTE,
            1,
            2,
            OPENAI_ROLLBACK_SUCCEEDED) ||
        !openai_m123_write_invalid(path) ||
        !openai_m123_load_status(
            path,
            &status)) {
        (void)openai_state_purge_all(path);
        return 0;
    }

    (void)openai_state_purge_all(path);
    return status == 2;
}

int openai_m123_both_invalid(
    const char *path)
{
    int workflow;
    int build_known;
    int build_status;
    int rollback;

    (void)openai_state_purge_all(path);

    if (!openai_m123_write_invalid(path) ||
        !openai_m123_write_invalid(path)) {
        (void)openai_state_purge_all(path);
        return 0;
    }

    if (openai_state_load_recovery(
            path,
            &workflow,
            &build_known,
            &build_status,
            &rollback)) {
        (void)openai_state_purge_all(path);
        return 0;
    }

    (void)openai_state_purge_all(path);
    return 1;
}

int openai_m123_read_only(
    const char *path)
{
    char prior_path[OPENAI_STATE_PATH_SIZE];
    int count;
    int newest_before;
    int prior_before;
    int loaded;
    int newest_after;
    int prior_after;

    (void)openai_state_purge_all(path);

    if (!openai_state_save_to(
            path,
            OPENAI_WORKFLOW_EXECUTE,
            1,
            1,
            OPENAI_ROLLBACK_SUCCEEDED) ||
        !openai_state_save_to(
            path,
            OPENAI_WORKFLOW_EXECUTE,
            1,
            2,
            OPENAI_ROLLBACK_SUCCEEDED)) {
        (void)openai_state_purge_all(path);
        return 0;
    }

    count = snprintf(
        prior_path,
        sizeof(prior_path),
        "%s;-1",
        path);

    if (count < 0 ||
        (size_t)count >= sizeof(prior_path) ||
        !openai_m122_load_status(path, &newest_before) ||
        !openai_m122_load_status(prior_path, &prior_before) ||
        !openai_m123_load_status(path, &loaded) ||
        !openai_m122_load_status(path, &newest_after) ||
        !openai_m122_load_status(prior_path, &prior_after)) {
        (void)openai_state_purge_all(path);
        return 0;
    }

    (void)openai_state_purge_all(path);

    return
        newest_before == 2 &&
        prior_before == 1 &&
        loaded == 2 &&
        newest_after == 2 &&
        prior_after == 1;
}
