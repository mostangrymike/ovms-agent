static int openai_m114_write_state(
    const char *path,
    int build_known,
    int build_status,
    int expect_accept)
{
    FILE *file;
    int workflow;
    int loaded_known;
    int loaded_status;
    int rollback;
    int loaded;

    if (path == NULL || *path == '\0') {
        return 0;
    }

    file = fopen(path, "w");

    if (file == NULL) {
        return 0;
    }

    if (fprintf(
            file,
            "format=1\n"
            "last_workflow=13\n"
            "last_build_known=%d\n"
            "last_build_status=%d\n"
            "last_rollback=2\n",
            build_known,
            build_status) < 0 ||
        fclose(file) != 0) {
        return 0;
    }

    loaded = openai_state_load_from(
        path,
        &workflow,
        &loaded_known,
        &loaded_status,
        &rollback);

    if (!expect_accept) {
        return !loaded;
    }

    return
        loaded &&
        workflow == OPENAI_WORKFLOW_EXECUTE &&
        loaded_known == build_known &&
        loaded_status == build_status &&
        rollback == OPENAI_ROLLBACK_SUCCEEDED;
}

int openai_m114_unknown_nonzero(
    const char *path)
{
    return openai_m114_write_state(
        path,
        0,
        2,
        0);
}

int openai_m114_unknown_zero(
    const char *path)
{
    return openai_m114_write_state(
        path,
        0,
        0,
        1);
}

int openai_m114_known_success(
    const char *path)
{
    return openai_m114_write_state(
        path,
        1,
        1,
        1);
}

int openai_m114_known_failure(
    const char *path)
{
    return openai_m114_write_state(
        path,
        1,
        2,
        1);
}
