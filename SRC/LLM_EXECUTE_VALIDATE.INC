int openai_exec_validate_ops_file(const char *path)
{
    openai_saved_operation *operations;
    unsigned int operation_count;
    int result;

    if (path == NULL || *path == '\0') {
        return 0;
    }

    operations = (openai_saved_operation *)calloc(
        OPENAI_EXECUTE_MAX_OPERATIONS,
        sizeof(*operations));

    if (operations == NULL) {
        return 0;
    }

    operation_count = 0U;
    result = execute_parse_operations(
        path,
        operations,
        OPENAI_EXECUTE_MAX_OPERATIONS,
        &operation_count);

    free(operations);
    return result;
}
