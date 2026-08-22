int llm_m107_build_evidence(const char *path)
{
    char *text;
    int result;

    result = execute_save_build_to(
        path,
        "SIMULATED BUILD FAILURE\n");

    if (!result) {
        return 0;
    }

    text = llm_read_text_file(path);
    result =
        text != NULL &&
        strstr(text, "SIMULATED BUILD FAILURE") != NULL;
    free(text);
    return result;
}

int llm_m107_ops_evidence(const char *path)
{
    llm_saved_operation operation;
    char *text;
    int result;

    (void)memset(&operation, 0, sizeof(operation));
    (void)strcpy(
        operation.path,
        "SRC/LLM_M107_TEST.C");
    (void)strcpy(operation.old_text, "ORIGINAL");
    (void)strcpy(operation.new_text, "BROKEN");
    operation.is_block = 1;

    result = execute_save_ops_to(
        path,
        &operation,
        1U);

    if (!result) {
        return 0;
    }

    text = llm_read_text_file(path);
    result =
        text != NULL &&
        strstr(text, "operation_count=1") != NULL &&
        strstr(text, "BEGIN_OLD_TEXT") != NULL &&
        strstr(text, "ORIGINAL") != NULL &&
        strstr(text, "BEGIN_NEW_TEXT") != NULL &&
        strstr(text, "BROKEN") != NULL &&
        strstr(text, "END_OPERATION") != NULL;
    free(text);
    return result;
}

int llm_m108_build_writer(const char *path)
{
    return execute_save_build_to(
        path,
        "M108 PRODUCTION BUILD EVIDENCE\n");
}

int llm_m108_ops_writer(const char *path)
{
    llm_saved_operation operation;

    (void)memset(&operation, 0, sizeof(operation));
    (void)strcpy(operation.path, "SRC/M108.C");
    (void)strcpy(operation.old_text, "OLD");
    (void)strcpy(operation.new_text, "NEW");
    operation.is_block = 1;

    return execute_save_ops_to(
        path,
        &operation,
        1U);
}

int llm_m108_null_path(void)
{
    return
        !execute_save_build_to(
            NULL,
            "M108") &&
        !execute_save_ops_to(
            NULL,
            NULL,
            0U);
}

int llm_m108_null_ops(const char *path)
{
    return !execute_save_ops_to(
        path,
        NULL,
        1U);
}
