static void llm_state_note_save_result(
    int save_succeeded)
{
    llm_state_loaded = 1;
    llm_state_valid = save_succeeded ? 1 : 0;
}

int llm_m121_success_flags(void)
{
    int saved_loaded;
    int saved_valid;
    int result;

    saved_loaded = llm_state_loaded;
    saved_valid = llm_state_valid;

    llm_state_loaded = 0;
    llm_state_valid = 0;
    llm_state_note_save_result(1);

    result =
        llm_state_loaded == 1 &&
        llm_state_valid == 1;

    llm_state_loaded = saved_loaded;
    llm_state_valid = saved_valid;
    return result;
}

int llm_m121_failure_valid(void)
{
    int saved_loaded;
    int saved_valid;
    int result;

    saved_loaded = llm_state_loaded;
    saved_valid = llm_state_valid;

    llm_state_loaded = 1;
    llm_state_valid = 1;
    llm_state_note_save_result(0);

    result = llm_state_valid == 0;

    llm_state_loaded = saved_loaded;
    llm_state_valid = saved_valid;
    return result;
}

int llm_m121_failure_loaded(void)
{
    int saved_loaded;
    int saved_valid;
    int result;

    saved_loaded = llm_state_loaded;
    saved_valid = llm_state_valid;

    llm_state_loaded = 0;
    llm_state_valid = 1;
    llm_state_note_save_result(0);

    result =
        llm_state_loaded == 1 &&
        llm_state_valid == 0;

    llm_state_loaded = saved_loaded;
    llm_state_valid = saved_valid;
    return result;
}

int llm_m121_disk_preserved(
    const char *path)
{
    int workflow;
    int build_known;
    int build_status;
    int rollback;

    (void)llm_state_purge_all(path);

    if (!llm_state_save_to(
            path,
            LLM_WORKFLOW_EXECUTE,
            1,
            1,
            LLM_ROLLBACK_SUCCEEDED)) {
        return 0;
    }

    if (llm_state_save_to(
            path,
            LLM_WORKFLOW_EXECUTE,
            1,
            -1,
            LLM_ROLLBACK_SUCCEEDED)) {
        (void)llm_state_purge_all(path);
        return 0;
    }

    if (!llm_state_load_from(
            path,
            &workflow,
            &build_known,
            &build_status,
            &rollback)) {
        (void)llm_state_purge_all(path);
        return 0;
    }

    (void)llm_state_purge_all(path);

    return
        workflow == LLM_WORKFLOW_EXECUTE &&
        build_known == 1 &&
        build_status == 1 &&
        rollback == LLM_ROLLBACK_SUCCEEDED;
}
