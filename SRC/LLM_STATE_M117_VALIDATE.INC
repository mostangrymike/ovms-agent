int openai_m117_tmp_is_unique(
    const char *path)
{
    char temporary_path[OPENAI_STATE_PATH_SIZE];
    char expected[64];

    if (!openai_state_make_tmp_path(
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

int openai_m117_tmp_truncation(
    const char *path)
{
    char temporary_path[8];

    return !openai_state_make_tmp_path(
        path,
        temporary_path,
        sizeof(temporary_path));
}

int openai_m117_tmp_no_collision(
    const char *path)
{
    char first[OPENAI_STATE_PATH_SIZE];
    char second[OPENAI_STATE_PATH_SIZE];
    char second_path[OPENAI_STATE_PATH_SIZE];
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
        openai_state_make_tmp_path(
            path,
            first,
            sizeof(first)) &&
        openai_state_make_tmp_path(
            second_path,
            second,
            sizeof(second)) &&
        strcmp(first, second) != 0;
}

int openai_m117_atomic_valid(
    const char *path)
{
    int workflow;
    int build_known;
    int build_status;
    int rollback;

    if (!openai_state_save_to(
            path,
            OPENAI_WORKFLOW_EXECUTE,
            1,
            1,
            OPENAI_ROLLBACK_SUCCEEDED)) {
        return 0;
    }

    return
        openai_state_load_from(
            path,
            &workflow,
            &build_known,
            &build_status,
            &rollback) &&
        workflow == OPENAI_WORKFLOW_EXECUTE &&
        build_known == 1 &&
        build_status == 1 &&
        rollback == OPENAI_ROLLBACK_SUCCEEDED;
}
