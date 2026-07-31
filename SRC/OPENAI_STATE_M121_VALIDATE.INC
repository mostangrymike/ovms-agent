static void openai_state_note_save_result(
    int save_succeeded)
{
    openai_state_loaded = 1;
    openai_state_valid = save_succeeded ? 1 : 0;
}

int openai_m121_success_flags(void)
{
    int saved_loaded;
    int saved_valid;
    int result;

    saved_loaded = openai_state_loaded;
    saved_valid = openai_state_valid;

    openai_state_loaded = 0;
    openai_state_valid = 0;
    openai_state_note_save_result(1);

    result =
        openai_state_loaded == 1 &&
        openai_state_valid == 1;

    openai_state_loaded = saved_loaded;
    openai_state_valid = saved_valid;
    return result;
}

int openai_m121_failure_valid(void)
{
    int saved_loaded;
    int saved_valid;
    int result;

    saved_loaded = openai_state_loaded;
    saved_valid = openai_state_valid;

    openai_state_loaded = 1;
    openai_state_valid = 1;
    openai_state_note_save_result(0);

    result = openai_state_valid == 0;

    openai_state_loaded = saved_loaded;
    openai_state_valid = saved_valid;
    return result;
}

int openai_m121_failure_loaded(void)
{
    int saved_loaded;
    int saved_valid;
    int result;

    saved_loaded = openai_state_loaded;
    saved_valid = openai_state_valid;

    openai_state_loaded = 0;
    openai_state_valid = 1;
    openai_state_note_save_result(0);

    result =
        openai_state_loaded == 1 &&
        openai_state_valid == 0;

    openai_state_loaded = saved_loaded;
    openai_state_valid = saved_valid;
    return result;
}

int openai_m121_disk_preserved(
    const char *path)
{
    int workflow;
    int build_known;
    int build_status;
    int rollback;

    (void)openai_state_purge_all(path);

    if (!openai_state_save_to(
            path,
            OPENAI_WORKFLOW_EXECUTE,
            1,
            1,
            OPENAI_ROLLBACK_SUCCEEDED)) {
        return 0;
    }

    if (openai_state_save_to(
            path,
            OPENAI_WORKFLOW_EXECUTE,
            1,
            -1,
            OPENAI_ROLLBACK_SUCCEEDED)) {
        (void)openai_state_purge_all(path);
        return 0;
    }

    if (!openai_state_load_from(
            path,
            &workflow,
            &build_known,
            &build_status,
            &rollback)) {
        (void)openai_state_purge_all(path);
        return 0;
    }

    (void)openai_state_purge_all(path);

    return
        workflow == OPENAI_WORKFLOW_EXECUTE &&
        build_known == 1 &&
        build_status == 1 &&
        rollback == OPENAI_ROLLBACK_SUCCEEDED;
}
