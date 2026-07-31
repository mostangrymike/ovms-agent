int openai_m107_state_serialize(const char *path)
{
    char *text;
    int result;

    result = openai_state_save_to(
        path,
        OPENAI_WORKFLOW_EXECUTE,
        1,
        2,
        OPENAI_ROLLBACK_SUCCEEDED);

    if (!result) {
        return 0;
    }

    text = openai_read_text_file(path);
    result =
        text != NULL &&
        strstr(text, "last_workflow=13") != NULL &&
        strstr(text, "last_rollback=2") != NULL;
    free(text);
    return result;
}

int openai_m107_state_reload(const char *path)
{
    int workflow;
    int build_known;
    int build_status;
    int rollback;

    if (!openai_m107_state_serialize(path)) {
        return 0;
    }

    if (!openai_state_load_from(
            path,
            &workflow,
            &build_known,
            &build_status,
            &rollback)) {
        return 0;
    }

    return
        workflow == OPENAI_WORKFLOW_EXECUTE &&
        build_known == 1 &&
        build_status == 2 &&
        rollback == OPENAI_ROLLBACK_SUCCEEDED;
}

int openai_m109_state_writer(const char *path)
{
    return openai_state_save_to(
        path,
        OPENAI_WORKFLOW_EXECUTE,
        1,
        2,
        OPENAI_ROLLBACK_SUCCEEDED);
}

int openai_m109_state_reader(const char *path)
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
        build_status == 2 &&
        rollback == OPENAI_ROLLBACK_SUCCEEDED;
}

int openai_m109_invalid_workflow(const char *path)
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

    if (fprintf(
            file,
            "format=1\n"
            "last_workflow=14\n"
            "last_build_known=1\n"
            "last_build_status=2\n"
            "last_rollback=2\n") < 0 ||
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

int openai_m109_incomplete_state(const char *path)
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

    if (fprintf(
            file,
            "format=1\n"
            "last_workflow=13\n"
            "last_build_known=1\n"
            "last_build_status=2\n") < 0 ||
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


static int openai_m110_write_and_reject(
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

int openai_m110_dup_workflow(const char *path)
{
    return openai_m110_write_and_reject(
        path,
        "format=1\n"
        "last_workflow=13\n"
        "last_workflow=12\n"
        "last_build_known=1\n"
        "last_build_status=2\n"
        "last_rollback=2\n");
}

int openai_m110_dup_rollback(const char *path)
{
    return openai_m110_write_and_reject(
        path,
        "format=1\n"
        "last_workflow=13\n"
        "last_build_known=1\n"
        "last_build_status=2\n"
        "last_rollback=2\n"
        "last_rollback=3\n");
}

int openai_m110_unknown_field(const char *path)
{
    return openai_m110_write_and_reject(
        path,
        "format=1\n"
        "last_workflow=13\n"
        "last_build_known=1\n"
        "last_build_status=2\n"
        "unexpected=1\n"
        "last_rollback=2\n");
}

int openai_m110_value_suffix(const char *path)
{
    return openai_m110_write_and_reject(
        path,
        "format=1\n"
        "last_workflow=13x\n"
        "last_build_known=1\n"
        "last_build_status=2\n"
        "last_rollback=2\n");
}
