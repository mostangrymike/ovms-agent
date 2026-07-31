static int openai_m111_write_and_reject(
    const char *path,
    const char *text)
{
    FILE *file;
    int workflow;
    int build_known;
    int build_status;
    int rollback;

    if (path == NULL ||
        *path == '\0' ||
        text == NULL) {
        return 0;
    }

    file = fopen(path, "w");

    if (file == NULL) {
        return 0;
    }

    if (fputs(text, file) == EOF ||
        fclose(file) != 0) {
        return 0;
    }

    return !openai_state_load_from(
        path,
        &workflow,
        &build_known,
        &build_status,
        &rollback);
}

int openai_m111_workflow_first(
    const char *path)
{
    return openai_m111_write_and_reject(
        path,
        "last_workflow=13\n"
        "format=1\n"
        "last_build_known=1\n"
        "last_build_status=2\n"
        "last_rollback=2\n");
}

int openai_m111_status_before_known(
    const char *path)
{
    return openai_m111_write_and_reject(
        path,
        "format=1\n"
        "last_workflow=13\n"
        "last_build_status=2\n"
        "last_build_known=1\n"
        "last_rollback=2\n");
}

int openai_m111_rollback_first(
    const char *path)
{
    return openai_m111_write_and_reject(
        path,
        "format=1\n"
        "last_workflow=13\n"
        "last_build_known=1\n"
        "last_rollback=2\n"
        "last_build_status=2\n");
}

int openai_m111_canonical_order(
    const char *path)
{
    int workflow;
    int build_known;
    int build_status;
    int rollback;

    if (!openai_state_save_to(
            path,
            OPENAI_WORKFLOW_EXECUTE,
            1,
            2,
            OPENAI_ROLLBACK_SUCCEEDED)) {
        return 0;
    }

    return
        openai_state_load_from(
            path,
            &workflow,
            &build_known,
            &build_status,
            &rollback) &&
        workflow == OPENAI_WORKFLOW_EXECUTE &&
        build_known == 1 &&
        build_status == 2 &&
        rollback == OPENAI_ROLLBACK_SUCCEEDED;
}
