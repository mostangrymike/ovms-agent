static int openai_m112_load_rejected(
    const char *path)
{
    int workflow;
    int build_known;
    int build_status;
    int rollback;

    return !openai_state_load_from(
        path,
        &workflow,
        &build_known,
        &build_status,
        &rollback);
}

int openai_m112_big_workflow(
    const char *path)
{
    FILE *file;
    int index;

    if (path == NULL || *path == '\0') {
        return 0;
    }

    file = fopen(path, "w");

    if (file == NULL) {
        return 0;
    }

    if (fputs(
            "format=1\n"
            "last_workflow=13",
            file) == EOF) {
        (void)fclose(file);
        return 0;
    }

    for (index = 0; index < 300; ++index) {
        if (fputc('7', file) == EOF) {
            (void)fclose(file);
            return 0;
        }
    }

    if (fputs(
            "\n"
            "last_build_known=1\n"
            "last_build_status=2\n"
            "last_rollback=2\n",
            file) == EOF ||
        fclose(file) != 0) {
        return 0;
    }

    return openai_m112_load_rejected(path);
}

int openai_m112_big_unknown(
    const char *path)
{
    FILE *file;
    int index;

    if (path == NULL || *path == '\0') {
        return 0;
    }

    file = fopen(path, "w");

    if (file == NULL) {
        return 0;
    }

    if (fputs(
            "format=1\n"
            "last_workflow=13\n",
            file) == EOF) {
        (void)fclose(file);
        return 0;
    }

    for (index = 0; index < 300; ++index) {
        if (fputc('X', file) == EOF) {
            (void)fclose(file);
            return 0;
        }
    }

    if (fputs(
            "\n"
            "last_build_known=1\n"
            "last_build_status=2\n"
            "last_rollback=2\n",
            file) == EOF ||
        fclose(file) != 0) {
        return 0;
    }

    return openai_m112_load_rejected(path);
}

int openai_m112_unterm_middle(
    const char *path)
{
    FILE *file;

    if (path == NULL || *path == '\0') {
        return 0;
    }

    file = fopen(path, "w");

    if (file == NULL) {
        return 0;
    }

    if (fputs(
            "format=1"
            "last_workflow=13\n"
            "last_build_known=1\n"
            "last_build_status=2\n"
            "last_rollback=2\n",
            file) == EOF ||
        fclose(file) != 0) {
        return 0;
    }

    return openai_m112_load_rejected(path);
}

int openai_m112_unterm_final(
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
            "last_workflow=13\n"
            "last_build_known=1\n"
            "last_build_status=2\n"
            "last_rollback=2",
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
        workflow == OPENAI_WORKFLOW_EXECUTE &&
        build_known == 1 &&
        build_status == 2 &&
        rollback == OPENAI_ROLLBACK_SUCCEEDED;
}
