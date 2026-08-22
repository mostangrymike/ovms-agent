static int llm_m122_make_version_path(
    const char *path,
    const char *suffix,
    char *version_path,
    size_t version_size)
{
    int count;

    if (path == NULL ||
        *path == '\0' ||
        suffix == NULL ||
        *suffix == '\0' ||
        version_path == NULL ||
        version_size == 0U) {
        return 0;
    }

    count = snprintf(
        version_path,
        version_size,
        "%s%s",
        path,
        suffix);

    return
        count >= 0 &&
        (size_t)count < version_size;
}

static int llm_m122_load_status(
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

    if (!llm_state_load_from(
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

int llm_m122_zero_is_newest(
    const char *path)
{
    char version_path[LLM_STATE_PATH_SIZE];
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
        !llm_m122_make_version_path(
            path,
            ";0",
            version_path,
            sizeof(version_path)) ||
        !llm_m122_load_status(
            version_path,
            &status)) {
        (void)llm_state_purge_all(path);
        return 0;
    }

    (void)llm_state_purge_all(path);
    return status == 2;
}

int llm_m122_minus_one_prior(
    const char *path)
{
    char version_path[LLM_STATE_PATH_SIZE];
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
        !llm_m122_make_version_path(
            path,
            ";-1",
            version_path,
            sizeof(version_path)) ||
        !llm_m122_load_status(
            version_path,
            &status)) {
        (void)llm_state_purge_all(path);
        return 0;
    }

    (void)llm_state_purge_all(path);
    return status == 1;
}

int llm_m122_missing_prior_fails(
    const char *path)
{
    char version_path[LLM_STATE_PATH_SIZE];
    int status;

    (void)llm_state_purge_all(path);

    if (!llm_state_save_to(
            path,
            LLM_WORKFLOW_EXECUTE,
            1,
            1,
            LLM_ROLLBACK_SUCCEEDED) ||
        !llm_m122_make_version_path(
            path,
            ";-1",
            version_path,
            sizeof(version_path))) {
        (void)llm_state_purge_all(path);
        return 0;
    }

    if (llm_m122_load_status(
            version_path,
            &status)) {
        (void)llm_state_purge_all(path);
        return 0;
    }

    (void)llm_state_purge_all(path);
    return 1;
}

int llm_m122_probe_read_only(
    const char *path)
{
    char version_path[LLM_STATE_PATH_SIZE];
    int newest_before;
    int newest_after;
    int prior_before;
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
            LLM_ROLLBACK_SUCCEEDED) ||
        !llm_m122_load_status(
            path,
            &newest_before) ||
        !llm_m122_make_version_path(
            path,
            ";-1",
            version_path,
            sizeof(version_path)) ||
        !llm_m122_load_status(
            version_path,
            &prior_before) ||
        !llm_m122_load_status(
            path,
            &newest_after) ||
        !llm_m122_load_status(
            version_path,
            &prior_after)) {
        (void)llm_state_purge_all(path);
        return 0;
    }

    (void)llm_state_purge_all(path);

    return
        newest_before == 2 &&
        newest_after == 2 &&
        prior_before == 1 &&
        prior_after == 1;
}
