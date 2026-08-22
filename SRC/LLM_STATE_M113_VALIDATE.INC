static int openai_m113_write_reject(
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

int openai_m113_signed_workflow(
    const char *path)
{
    return openai_m113_write_reject(
        path,
        "format=1\n"
        "last_workflow=+13\n"
        "last_build_known=1\n"
        "last_build_status=2\n"
        "last_rollback=2\n");
}

int openai_m113_zero_workflow(
    const char *path)
{
    return openai_m113_write_reject(
        path,
        "format=1\n"
        "last_workflow=013\n"
        "last_build_known=1\n"
        "last_build_status=2\n"
        "last_rollback=2\n");
}

int openai_m113_spaced_value(
    const char *path)
{
    return openai_m113_write_reject(
        path,
        "format=1\n"
        "last_workflow= 13\n"
        "last_build_known=1\n"
        "last_build_status=2\n"
        "last_rollback=2\n");
}

int openai_m113_zero_values(
    const char *path)
{
    FILE *file;
    int workflow;
    int build_known;
    int build_status;
    int rollback;

    if (path == NULL || *path == '\0') {
        return 0;
    }

    file = fopen(path, "w");

    if (file == NULL) {
        return 0;
    }

    if (fputs(
            "format=1\n"
            "last_workflow=0\n"
            "last_build_known=0\n"
            "last_build_status=0\n"
            "last_rollback=0\n",
            file) == EOF ||
        fclose(file) != 0) {
        return 0;
    }

    return
        openai_state_load_from(
            path,
            &workflow,
            &build_known,
            &build_status,
            &rollback) &&
        workflow == OPENAI_WORKFLOW_NONE &&
        build_known == 0 &&
        build_status == 0 &&
        rollback == OPENAI_ROLLBACK_NONE;
}
