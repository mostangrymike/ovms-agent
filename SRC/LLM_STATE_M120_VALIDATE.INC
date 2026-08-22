int llm_m120_recovery_version(
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

    if (remove(path) != 0) {
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

int llm_m120_versions_bounded(
    const char *path)
{
    int index;
    int removed;

    (void)llm_state_purge_all(path);

    for (index = 1; index <= 4; ++index) {
        if (!llm_state_save_to(
                path,
                LLM_WORKFLOW_EXECUTE,
                1,
                index,
                LLM_ROLLBACK_SUCCEEDED)) {
            (void)llm_state_purge_all(path);
            return 0;
        }
    }

    removed = 0;

    while (remove(path) == 0) {
        ++removed;
    }

    return removed == 2 && errno == ENOENT;
}

int llm_m120_newest_preserved(
    const char *path)
{
    int workflow;
    int build_known;
    int build_status;
    int rollback;
    int index;
    int valid;

    (void)llm_state_purge_all(path);

    for (index = 1; index <= 4; ++index) {
        if (!llm_state_save_to(
                path,
                LLM_WORKFLOW_EXECUTE,
                1,
                index,
                LLM_ROLLBACK_SUCCEEDED)) {
            (void)llm_state_purge_all(path);
            return 0;
        }
    }

    valid =
        llm_state_load_from(
            path,
            &workflow,
            &build_known,
            &build_status,
            &rollback) &&
        workflow == LLM_WORKFLOW_EXECUTE &&
        build_known == 1 &&
        build_status == 4 &&
        rollback == LLM_ROLLBACK_SUCCEEDED;

    (void)llm_state_purge_all(path);
    return valid;
}

int llm_m120_unsafe_rejected(void)
{
    return
        !llm_state_prune_versions("../BAD.STATE") &&
        !llm_state_prune_versions("BAD;1") &&
        !llm_state_prune_versions("SYS$DISK:[BAD]STATE.DAT") &&
        !llm_state_prune_versions("BAD STATE.DAT");
}
