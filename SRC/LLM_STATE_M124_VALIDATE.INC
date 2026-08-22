int llm_m124_normal_load(
    const char *path)
{
    int workflow;
    int build_known;
    int build_status;
    int rollback;
    int recovered;

    (void)llm_state_purge_all(path);

    if (!llm_state_save_to(
            path,
            LLM_WORKFLOW_EXECUTE,
            1,
            1,
            LLM_ROLLBACK_SUCCEEDED) ||
        !llm_state_load_recovery_flag(
            path,
            &workflow,
            &build_known,
            &build_status,
            &rollback,
            &recovered)) {
        (void)llm_state_purge_all(path);
        return 0;
    }

    (void)llm_state_purge_all(path);

    return
        recovered == 0 &&
        build_status == 1;
}

int llm_m124_fallback_marked(
    const char *path)
{
    FILE *file;
    int workflow;
    int build_known;
    int build_status;
    int rollback;
    int recovered;

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

    file = fopen(path, "w");

    if (file == NULL) {
        (void)llm_state_purge_all(path);
        return 0;
    }

    if (fputs("invalid\n", file) == EOF ||
        fclose(file) != 0) {
        (void)llm_state_purge_all(path);
        return 0;
    }

    if (!llm_state_load_recovery_flag(
            path,
            &workflow,
            &build_known,
            &build_status,
            &rollback,
            &recovered)) {
        (void)llm_state_purge_all(path);
        return 0;
    }

    (void)llm_state_purge_all(path);

    return
        recovered == 1 &&
        build_status == 2;
}

int llm_m124_save_clears(void)
{
    int saved;

    saved = llm_state_recovered;
    llm_state_recovered = 1;
    llm_state_note_save_result(1);

    if (llm_state_valid) {
        llm_state_recovered = 0;
    }

    if (llm_state_recovered != 0) {
        llm_state_recovered = saved;
        return 0;
    }

    llm_state_recovered = saved;
    return 1;
}

int llm_m124_clear_clears(void)
{
    int saved;

    saved = llm_state_recovered;
    llm_state_recovered = 1;
    llm_state_recovered = 0;

    if (llm_state_recovered != 0) {
        llm_state_recovered = saved;
        return 0;
    }

    llm_state_recovered = saved;
    return 1;
}
