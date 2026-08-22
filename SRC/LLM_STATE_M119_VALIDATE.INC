static int llm_m119_write_raw(
    const char *path,
    const char *status_text)
{
    FILE *file;
    int success;

    if (path == NULL ||
        *path == '\0' ||
        status_text == NULL) {
        return 0;
    }

    file = fopen(path, "w");

    if (file == NULL) {
        return 0;
    }

    success =
        fprintf(
            file,
            "format=1\n"
            "last_workflow=%d\n"
            "last_build_known=1\n"
            "last_build_status=%s\n"
            "last_rollback=%d\n",
            LLM_WORKFLOW_EXECUTE,
            status_text,
            LLM_ROLLBACK_SUCCEEDED) >= 0;

    if (fclose(file) != 0) {
        success = 0;
    }

    return success;
}

int llm_m119_writer_negative(const char *path)
{
    return !llm_state_save_to(
        path,
        LLM_WORKFLOW_EXECUTE,
        1,
        -1,
        LLM_ROLLBACK_SUCCEEDED);
}

int llm_m119_loader_negative(const char *path)
{
    int workflow, build_known, build_status, rollback;

    if (!llm_m119_write_raw(path, "-1")) {
        return 0;
    }

    return !llm_state_load_from(
        path,
        &workflow,
        &build_known,
        &build_status,
        &rollback);
}

int llm_m119_known_zero(const char *path)
{
    int workflow, build_known, build_status, rollback;

    if (!llm_state_save_to(
            path,
            LLM_WORKFLOW_EXECUTE,
            1,
            0,
            LLM_ROLLBACK_SUCCEEDED)) {
        return 0;
    }

    return
        llm_state_load_from(
            path,
            &workflow,
            &build_known,
            &build_status,
            &rollback) &&
        workflow == LLM_WORKFLOW_EXECUTE &&
        build_known == 1 &&
        build_status == 0 &&
        rollback == LLM_ROLLBACK_SUCCEEDED;
}

int llm_m119_writer_reloads(const char *path)
{
    int workflow, build_known, build_status, rollback;

    if (!llm_state_save_to(
            path,
            LLM_WORKFLOW_EXECUTE,
            1,
            2,
            LLM_ROLLBACK_SUCCEEDED)) {
        return 0;
    }

    return
        llm_state_load_from(
            path,
            &workflow,
            &build_known,
            &build_status,
            &rollback) &&
        workflow == LLM_WORKFLOW_EXECUTE &&
        build_known == 1 &&
        build_status == 2 &&
        rollback == LLM_ROLLBACK_SUCCEEDED;
}
