static int llm_m123_write_invalid(
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

static int llm_m123_load_status(
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

    if (!llm_state_load_recovery(
            path,
            &workflow,
            &build_known,
            &build_status,
            &rollback)) {
        return 0;
    }

    if (workflow != LLM_WORKFLOW_EXECUTE ||
        build_known != 1 ||
        rollback != LLM_ROLLBACK_SUCCEEDED) {
        return 0;
    }

    *status_out = build_status;
    return 1;
}

int llm_m123_newest_preferred(
    const char *path)
{
    int status;

    (void)llm_state_purge_all(path);

    if (!llm_state_save_to(
            path,
            LLM_WORKFLOW_EXECUTE,
            1,
            1,
            LLM_ROLLBACK_SUCCEEDED) ||
        !llm_state_save_to(
            path,
            LLM_WORKFLOW_EXECUTE,
            1,
            2,
            LLM_ROLLBACK_SUCCEEDED) ||
        !llm_m123_load_status(
            path,
            &status)) {
        (void)llm_state_purge_all(path);
        return 0;
    }

    (void)llm_state_purge_all(path);
    return status == 2;
}

int llm_m123_fallback_prior(
    const char *path)
{
    int status;

    (void)llm_state_purge_all(path);

    if (!llm_state_save_to(
            path,
            LLM_WORKFLOW_EXECUTE,
            1,
            1,
            LLM_ROLLBACK_SUCCEEDED) ||
        !llm_state_save_to(
            path,
            LLM_WORKFLOW_EXECUTE,
            1,
            2,
            LLM_ROLLBACK_SUCCEEDED) ||
        !llm_m123_write_invalid(path) ||
        !llm_m123_load_status(
            path,
            &status)) {
        (void)llm_state_purge_all(path);
        return 0;
    }

    (void)llm_state_purge_all(path);
    return status == 2;
}

int llm_m123_both_invalid(
    const char *path)
{
    int workflow;
    int build_known;
    int build_status;
    int rollback;

    (void)llm_state_purge_all(path);

    if (!llm_m123_write_invalid(path) ||
        !llm_m123_write_invalid(path)) {
        (void)llm_state_purge_all(path);
        return 0;
    }

    if (llm_state_load_recovery(
            path,
            &workflow,
            &build_known,
            &build_status,
            &rollback)) {
        (void)llm_state_purge_all(path);
        return 0;
    }

    (void)llm_state_purge_all(path);
    return 1;
}

int llm_m123_read_only(
    const char *path)
{
    char prior_path[LLM_STATE_PATH_SIZE];
    int count;
    int newest_before;
    int prior_before;
    int loaded;
    int newest_after;
    int prior_after;

    (void)llm_state_purge_all(path);

    if (!llm_state_save_to(
            path,
            LLM_WORKFLOW_EXECUTE,
            1,
            1,
            LLM_ROLLBACK_SUCCEEDED) ||
        !llm_state_save_to(
            path,
            LLM_WORKFLOW_EXECUTE,
            1,
            2,
            LLM_ROLLBACK_SUCCEEDED)) {
        (void)llm_state_purge_all(path);
        return 0;
    }

    count = snprintf(
        prior_path,
        sizeof(prior_path),
        "%s;-1",
        path);

    if (count < 0 ||
        (size_t)count >= sizeof(prior_path) ||
        !llm_m122_load_status(path, &newest_before) ||
        !llm_m122_load_status(prior_path, &prior_before) ||
        !llm_m123_load_status(path, &loaded) ||
        !llm_m122_load_status(path, &newest_after) ||
        !llm_m122_load_status(prior_path, &prior_after)) {
        (void)llm_state_purge_all(path);
        return 0;
    }

    (void)llm_state_purge_all(path);

    return
        newest_before == 2 &&
        prior_before == 1 &&
        loaded == 2 &&
        newest_after == 2 &&
        prior_after == 1;
}
