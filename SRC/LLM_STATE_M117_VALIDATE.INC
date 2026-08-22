int llm_m117_tmp_is_unique(
    const char *path)
{
    char temporary_path[LLM_STATE_PATH_SIZE];
    char expected[64];

    if (!llm_state_make_tmp_path(
            path,
            temporary_path,
            sizeof(temporary_path))) {
        return 0;
    }

    if (snprintf(
            expected,
            sizeof(expected),
            "_%lu.TMP",
            (unsigned long)getpid()) < 0) {
        return 0;
    }

    return
        strcmp(path, temporary_path) != 0 &&
        strstr(temporary_path, expected) != NULL;
}

int llm_m117_tmp_truncation(
    const char *path)
{
    char temporary_path[8];

    return !llm_state_make_tmp_path(
        path,
        temporary_path,
        sizeof(temporary_path));
}

int llm_m117_tmp_no_collision(
    const char *path)
{
    char first[LLM_STATE_PATH_SIZE];
    char second[LLM_STATE_PATH_SIZE];
    char second_path[LLM_STATE_PATH_SIZE];
    int count;

    count = snprintf(
        second_path,
        sizeof(second_path),
        "%s_ALT",
        path);

    if (count < 0 ||
        (size_t)count >= sizeof(second_path)) {
        return 0;
    }

    return
        llm_state_make_tmp_path(
            path,
            first,
            sizeof(first)) &&
        llm_state_make_tmp_path(
            second_path,
            second,
            sizeof(second)) &&
        strcmp(first, second) != 0;
}

int llm_m117_atomic_valid(
    const char *path)
{
    int workflow;
    int build_known;
    int build_status;
    int rollback;

    if (!llm_state_save_to(
            path,
            LLM_WORKFLOW_EXECUTE,
            1,
            1,
            LLM_ROLLBACK_SUCCEEDED)) {
        return 0;
    }

    return
        llm_state_load_from(
            path,
            &workflow,
            &build_known,
            &build_status,
            &rollback) &&
        workflow == LLM_WORKFLOW_EXECUTE &&
        build_known == 1 &&
        build_status == 1 &&
        rollback == LLM_ROLLBACK_SUCCEEDED;
}
