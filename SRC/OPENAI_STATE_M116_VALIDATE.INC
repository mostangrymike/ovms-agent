static int openai_m116_read_state(
    const char *path,
    int expected_status)
{
    int workflow;
    int build_known;
    int build_status;
    int rollback;

    return
        openai_state_load_from(
            path,
            &workflow,
            &build_known,
            &build_status,
            &rollback) &&
        workflow == OPENAI_WORKFLOW_EXECUTE &&
        build_known == 1 &&
        build_status == expected_status &&
        rollback == OPENAI_ROLLBACK_SUCCEEDED;
}

int openai_m116_atomic_create(
    const char *path)
{
    return
        openai_state_save_to(
            path,
            OPENAI_WORKFLOW_EXECUTE,
            1,
            1,
            OPENAI_ROLLBACK_SUCCEEDED) &&
        openai_m116_read_state(path, 1);
}

int openai_m116_tmp_fail_keeps(
    const char *path)
{
    if (!openai_state_save_to(
            path,
            OPENAI_WORKFLOW_EXECUTE,
            1,
            1,
            OPENAI_ROLLBACK_SUCCEEDED)) {
        return 0;
    }

    if (openai_state_save_with_tmp(
            path,
            "",
            OPENAI_WORKFLOW_EXECUTE,
            1,
            2,
            OPENAI_ROLLBACK_SUCCEEDED)) {
        return 0;
    }

    return openai_m116_read_state(path, 1);
}

int openai_m116_bad_values_keep(
    const char *path)
{
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
            OPENAI_WORKFLOW_EXECUTE + 1,
            1,
            2,
            OPENAI_ROLLBACK_SUCCEEDED)) {
        return 0;
    }

    return openai_m116_read_state(path, 1);
}

int openai_m116_tmp_versions_clean(
    const char *path)
{
    const char *temporary_path =
        "OVMS_AGENT_TEST_M116_STALE.TMP";
    FILE *file;
    int index;

    openai_state_purge_file(temporary_path);

    for (index = 0; index < 3; ++index) {
        file = fopen(temporary_path, "w");

        if (file == NULL) {
            openai_state_purge_file(temporary_path);
            return 0;
        }

        if (fputs("stale\n", file) == EOF ||
            fclose(file) != 0) {
            openai_state_purge_file(temporary_path);
            return 0;
        }
    }

    if (!openai_state_save_with_tmp(
            path,
            temporary_path,
            OPENAI_WORKFLOW_EXECUTE,
            1,
            1,
            OPENAI_ROLLBACK_SUCCEEDED)) {
        openai_state_purge_file(temporary_path);
        return 0;
    }

    file = fopen(temporary_path, "r");

    if (file != NULL) {
        (void)fclose(file);
        openai_state_purge_file(temporary_path);
        return 0;
    }

    return openai_m116_read_state(path, 1);
}
