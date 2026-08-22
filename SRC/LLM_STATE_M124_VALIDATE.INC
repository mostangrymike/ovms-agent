int openai_m124_normal_load(
    const char *path)
{
    int workflow;
    int build_known;
    int build_status;
    int rollback;
    int recovered;

    (void)openai_state_purge_all(path);

    if (!openai_state_save_to(
            path,
            OPENAI_WORKFLOW_EXECUTE,
            1,
            1,
            OPENAI_ROLLBACK_SUCCEEDED) ||
        !openai_state_load_recovery_flag(
            path,
            &workflow,
            &build_known,
            &build_status,
            &rollback,
            &recovered)) {
        (void)openai_state_purge_all(path);
        return 0;
    }

    (void)openai_state_purge_all(path);

    return
        recovered == 0 &&
        build_status == 1;
}

int openai_m124_fallback_marked(
    const char *path)
{
    FILE *file;
    int workflow;
    int build_known;
    int build_status;
    int rollback;
    int recovered;

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

    file = fopen(path, "w");

    if (file == NULL) {
        (void)openai_state_purge_all(path);
        return 0;
    }

    if (fputs("invalid\n", file) == EOF ||
        fclose(file) != 0) {
        (void)openai_state_purge_all(path);
        return 0;
    }

    if (!openai_state_load_recovery_flag(
            path,
            &workflow,
            &build_known,
            &build_status,
            &rollback,
            &recovered)) {
        (void)openai_state_purge_all(path);
        return 0;
    }

    (void)openai_state_purge_all(path);

    return
        recovered == 1 &&
        build_status == 2;
}

int openai_m124_save_clears(void)
{
    int saved;

    saved = openai_state_recovered;
    openai_state_recovered = 1;
    openai_state_note_save_result(1);

    if (openai_state_valid) {
        openai_state_recovered = 0;
    }

    if (openai_state_recovered != 0) {
        openai_state_recovered = saved;
        return 0;
    }

    openai_state_recovered = saved;
    return 1;
}

int openai_m124_clear_clears(void)
{
    int saved;

    saved = openai_state_recovered;
    openai_state_recovered = 1;
    openai_state_recovered = 0;

    if (openai_state_recovered != 0) {
        openai_state_recovered = saved;
        return 0;
    }

    openai_state_recovered = saved;
    return 1;
}
