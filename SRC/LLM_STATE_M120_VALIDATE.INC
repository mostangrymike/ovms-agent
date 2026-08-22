int openai_m120_recovery_version(
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

    if (remove(path) != 0) {
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

int openai_m120_versions_bounded(
    const char *path)
{
    int index;
    int removed;

    (void)openai_state_purge_all(path);

    for (index = 1; index <= 4; ++index) {
        if (!openai_state_save_to(
                path,
                OPENAI_WORKFLOW_EXECUTE,
                1,
                index,
                OPENAI_ROLLBACK_SUCCEEDED)) {
            (void)openai_state_purge_all(path);
            return 0;
        }
    }

    removed = 0;

    while (remove(path) == 0) {
        ++removed;
    }

    return removed == 2 && errno == ENOENT;
}

int openai_m120_newest_preserved(
    const char *path)
{
    int workflow;
    int build_known;
    int build_status;
    int rollback;
    int index;
    int valid;

    (void)openai_state_purge_all(path);

    for (index = 1; index <= 4; ++index) {
        if (!openai_state_save_to(
                path,
                OPENAI_WORKFLOW_EXECUTE,
                1,
                index,
                OPENAI_ROLLBACK_SUCCEEDED)) {
            (void)openai_state_purge_all(path);
            return 0;
        }
    }

    valid =
        openai_state_load_from(
            path,
            &workflow,
            &build_known,
            &build_status,
            &rollback) &&
        workflow == OPENAI_WORKFLOW_EXECUTE &&
        build_known == 1 &&
        build_status == 4 &&
        rollback == OPENAI_ROLLBACK_SUCCEEDED;

    (void)openai_state_purge_all(path);
    return valid;
}

int openai_m120_unsafe_rejected(void)
{
    return
        !openai_state_prune_versions("../BAD.STATE") &&
        !openai_state_prune_versions("BAD;1") &&
        !openai_state_prune_versions("SYS$DISK:[BAD]STATE.DAT") &&
        !openai_state_prune_versions("BAD STATE.DAT");
}
