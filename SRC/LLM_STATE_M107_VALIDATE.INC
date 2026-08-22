int openai_m107_state_serialize(const char *path)
{
    FILE *file;
    char *text;
    int result;

    if (path == NULL || *path == '\0') {
        return 0;
    }

    file = fopen(path, "w");

    if (file == NULL) {
        return 0;
    }

    result =
        fprintf(
            file,
            "format=1\n"
            "last_workflow=%d\n"
            "last_build_known=1\n"
            "last_build_status=2\n"
            "last_rollback=%d\n",
            OPENAI_WORKFLOW_EXECUTE,
            OPENAI_ROLLBACK_SUCCEEDED) >= 0 &&
        fclose(file) == 0;

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
    FILE *file;
    char line[128];
    int format;
    int workflow;
    int build_known;
    int build_status;
    int rollback;
    int saw_format;
    int saw_workflow;
    int saw_build_known;
    int saw_build_status;
    int saw_rollback;

    if (!openai_m107_state_serialize(path)) {
        return 0;
    }

    file = fopen(path, "r");

    if (file == NULL) {
        return 0;
    }

    format = 0;
    workflow = OPENAI_WORKFLOW_NONE;
    build_known = 0;
    build_status = 0;
    rollback = OPENAI_ROLLBACK_NONE;
    saw_format = 0;
    saw_workflow = 0;
    saw_build_known = 0;
    saw_build_status = 0;
    saw_rollback = 0;

    while (fgets(line, sizeof(line), file) != NULL) {
        int value;

        if (sscanf(line, "format=%d", &value) == 1) {
            format = value;
            saw_format = 1;
        } else if (sscanf(line, "last_workflow=%d", &value) == 1) {
            workflow = value;
            saw_workflow = 1;
        } else if (sscanf(line, "last_build_known=%d", &value) == 1) {
            build_known = value;
            saw_build_known = 1;
        } else if (sscanf(line, "last_build_status=%d", &value) == 1) {
            build_status = value;
            saw_build_status = 1;
        } else if (sscanf(line, "last_rollback=%d", &value) == 1) {
            rollback = value;
            saw_rollback = 1;
        }
    }

    (void)fclose(file);

    return
        saw_format &&
        saw_workflow &&
        saw_build_known &&
        saw_build_status &&
        saw_rollback &&
        format == 1 &&
        workflow >= OPENAI_WORKFLOW_NONE &&
        workflow <= OPENAI_WORKFLOW_EXECUTE &&
        workflow == OPENAI_WORKFLOW_EXECUTE &&
        build_known == 1 &&
        build_status == 2 &&
        rollback >= OPENAI_ROLLBACK_NONE &&
        rollback <= OPENAI_ROLLBACK_DECLINED &&
        rollback == OPENAI_ROLLBACK_SUCCEEDED;
}
