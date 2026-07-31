int openai_m118_purge_versions(
    const char *path)
{
    FILE *file;
    int index;

    if (path == NULL || *path == '\0') {
        return 0;
    }

    (void)openai_state_purge_all(path);

    for (index = 0; index < 3; ++index) {
        file = fopen(path, "w");

        if (file == NULL) {
            (void)openai_state_purge_all(path);
            return 0;
        }

        if (fprintf(file, "version=%d\n", index) < 0 ||
            fclose(file) != 0) {
            (void)openai_state_purge_all(path);
            return 0;
        }
    }

    if (!openai_state_purge_all(path)) {
        return 0;
    }

    file = fopen(path, "r");

    if (file != NULL) {
        (void)fclose(file);
        (void)openai_state_purge_all(path);
        return 0;
    }

    return 1;
}

int openai_m118_purge_missing(
    const char *path)
{
    if (path == NULL || *path == '\0') {
        return 0;
    }

    (void)openai_state_purge_all(path);

    return openai_state_purge_all(path);
}

int openai_m118_purge_empty(void)
{
    return
        !openai_state_purge_all(NULL) &&
        !openai_state_purge_all("");
}

int openai_m118_state_stays_clear(
    const char *path)
{
    FILE *file;
    int index;

    if (path == NULL || *path == '\0') {
        return 0;
    }

    (void)openai_state_purge_all(path);

    for (index = 0; index < 3; ++index) {
        if (!openai_state_save_with_tmp(
                path,
                "OVMS_AGENT_TEST_M118_WRITE.TMP",
                OPENAI_WORKFLOW_EXECUTE,
                1,
                index + 1,
                OPENAI_ROLLBACK_SUCCEEDED)) {
            (void)openai_state_purge_all(path);
            (void)openai_state_purge_all(
                "OVMS_AGENT_TEST_M118_WRITE.TMP");
            return 0;
        }
    }

    if (!openai_state_purge_all(path)) {
        (void)openai_state_purge_all(
            "OVMS_AGENT_TEST_M118_WRITE.TMP");
        return 0;
    }

    file = fopen(path, "r");

    if (file != NULL) {
        (void)fclose(file);
        (void)openai_state_purge_all(path);
        (void)openai_state_purge_all(
            "OVMS_AGENT_TEST_M118_WRITE.TMP");
        return 0;
    }

    (void)openai_state_purge_all(
        "OVMS_AGENT_TEST_M118_WRITE.TMP");
    return 1;
}
