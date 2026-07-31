int openai_exec_stage_expect_fail(
    const char *operations_path,
    const char *target_path,
    const char *expected_original_text)
{
    openai_saved_operation *operations;
    edit_txn *transaction;
    char *actual_text;
    unsigned int operation_count;
    int parse_result;
    int stage_result;
    int result;

    if (operations_path == NULL ||
        *operations_path == '\0' ||
        target_path == NULL ||
        *target_path == '\0' ||
        expected_original_text == NULL) {
        return 0;
    }

    operations = (openai_saved_operation *)calloc(
        OPENAI_EXECUTE_MAX_OPERATIONS,
        sizeof(*operations));
    transaction = (edit_txn *)malloc(sizeof(*transaction));

    if (operations == NULL || transaction == NULL) {
        free(transaction);
        free(operations);
        return 0;
    }

    operation_count = 0U;
    parse_result = execute_parse_operations(
        operations_path,
        operations,
        OPENAI_EXECUTE_MAX_OPERATIONS,
        &operation_count);

    result = 0;

    if (parse_result) {
        edit_txn_init(transaction);
        stage_result = execute_stage_operations_chained(
            transaction,
            operations,
            operation_count);
        edit_txn_dispose(transaction);

        actual_text = openai_read_text_file(target_path);

        if (!stage_result &&
            actual_text != NULL &&
            strcmp(actual_text, expected_original_text) == 0) {
            result = 1;
        }

        free(actual_text);
    }

    free(transaction);
    free(operations);
    return result;
}
