int llm_m118_purge_versions(
    const char *path)
{
    FILE *file;
    int index;

    if (path == NULL || *path == '\0') {
        return 0;
    }

    (void)llm_state_purge_all(path);

    for (index = 0; index < 3; ++index) {
        file = fopen(path, "w");

        if (file == NULL) {
            (void)llm_state_purge_all(path);
            return 0;
        }

        if (fprintf(file, "version=%d\n", index) < 0 ||
            fclose(file) != 0) {
            (void)llm_state_purge_all(path);
            return 0;
        }
    }

    if (!llm_state_purge_all(path)) {
        return 0;
    }

    file = fopen(path, "r");

    if (file != NULL) {
        (void)fclose(file);
        (void)llm_state_purge_all(path);
        return 0;
    }

    return 1;
}

int llm_m118_purge_missing(
    const char *path)
{
    if (path == NULL || *path == '\0') {
        return 0;
    }

    (void)llm_state_purge_all(path);

    return llm_state_purge_all(path);
}

int llm_m118_purge_empty(void)
{
    return
        !llm_state_purge_all(NULL) &&
        !llm_state_purge_all("");
}

int llm_m118_state_stays_clear(
    const char *path)
{
    FILE *file;
    int index;

    if (path == NULL || *path == '\0') {
        return 0;
    }

    (void)llm_state_purge_all(path);

    for (index = 0; index < 3; ++index) {
        if (!llm_state_save_with_tmp(
                path,
                "OVMS_AGENT_TEST_M118_WRITE.TMP",
                LLM_WORKFLOW_EXECUTE,
                1,
                index + 1,
                LLM_ROLLBACK_SUCCEEDED)) {
            (void)llm_state_purge_all(path);
            (void)llm_state_purge_all(
                "OVMS_AGENT_TEST_M118_WRITE.TMP");
            return 0;
        }
    }

    if (!llm_state_purge_all(path)) {
        (void)llm_state_purge_all(
            "OVMS_AGENT_TEST_M118_WRITE.TMP");
        return 0;
    }

    file = fopen(path, "r");

    if (file != NULL) {
        (void)fclose(file);
        (void)llm_state_purge_all(path);
        (void)llm_state_purge_all(
            "OVMS_AGENT_TEST_M118_WRITE.TMP");
        return 0;
    }

    (void)llm_state_purge_all(
        "OVMS_AGENT_TEST_M118_WRITE.TMP");
    return 1;
}
