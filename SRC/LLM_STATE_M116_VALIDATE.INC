static int llm_m116_read_state(
    const char *path,
    int expected_status)
{
    int workflow;
    int build_known;
    int build_status;
    int rollback;

    return
        llm_state_load_from(
            path,
            &workflow,
            &build_known,
            &build_status,
            &rollback) &&
        workflow == LLM_WORKFLOW_EXECUTE &&
        build_known == 1 &&
        build_status == expected_status &&
        rollback == LLM_ROLLBACK_SUCCEEDED;
}

int llm_m116_atomic_create(
    const char *path)
{
    return
        llm_state_save_to(
            path,
            LLM_WORKFLOW_EXECUTE,
            1,
            1,
            LLM_ROLLBACK_SUCCEEDED) &&
        llm_m116_read_state(path, 1);
}

int llm_m116_tmp_fail_keeps(
    const char *path)
{
    if (!llm_state_save_to(
            path,
            LLM_WORKFLOW_EXECUTE,
            1,
            1,
            LLM_ROLLBACK_SUCCEEDED)) {
        return 0;
    }

    if (llm_state_save_with_tmp(
            path,
            "",
            LLM_WORKFLOW_EXECUTE,
            1,
            2,
            LLM_ROLLBACK_SUCCEEDED)) {
        return 0;
    }

    return llm_m116_read_state(path, 1);
}

int llm_m116_bad_values_keep(
    const char *path)
{
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
            LLM_WORKFLOW_EXECUTE + 1,
            1,
            2,
            LLM_ROLLBACK_SUCCEEDED)) {
        return 0;
    }

    return llm_m116_read_state(path, 1);
}

int llm_m116_tmp_versions_clean(
    const char *path)
{
    const char *temporary_path =
        "OVMS_AGENT_TEST_M116_STALE.TMP";
    FILE *file;
    int index;

    llm_state_purge_file(temporary_path);

    for (index = 0; index < 3; ++index) {
        file = fopen(temporary_path, "w");

        if (file == NULL) {
            llm_state_purge_file(temporary_path);
            return 0;
        }

        if (fputs("stale\n", file) == EOF ||
            fclose(file) != 0) {
            llm_state_purge_file(temporary_path);
            return 0;
        }
    }

    if (!llm_state_save_with_tmp(
            path,
            temporary_path,
            LLM_WORKFLOW_EXECUTE,
            1,
            1,
            LLM_ROLLBACK_SUCCEEDED)) {
        llm_state_purge_file(temporary_path);
        return 0;
    }

    file = fopen(temporary_path, "r");

    if (file != NULL) {
        (void)fclose(file);
        llm_state_purge_file(temporary_path);
        return 0;
    }

    return llm_m116_read_state(path, 1);
}
