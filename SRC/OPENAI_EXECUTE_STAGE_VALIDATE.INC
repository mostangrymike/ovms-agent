int openai_exec_stage_ops_file(
    const char *path,
    const char *expected_text)
{
    openai_saved_operation *operations;
    edit_txn *transaction;
    unsigned int operation_count;
    int result;

    if (path == NULL ||
        *path == '\0' ||
        expected_text == NULL) {
        return 0;
    }

    operations = (openai_saved_operation *)calloc(
        OPENAI_EXECUTE_MAX_OPERATIONS,
        sizeof(*operations));

    transaction = (edit_txn *)malloc(sizeof(*transaction));

    if (operations == NULL ||
        transaction == NULL) {
        free(transaction);
        free(operations);
        return 0;
    }

    operation_count = 0U;
    result = execute_parse_operations(
        path,
        operations,
        OPENAI_EXECUTE_MAX_OPERATIONS,
        &operation_count);

    if (result) {
        edit_txn_init(transaction);

        result = execute_stage_operations_chained(
            transaction,
            operations,
            operation_count);

        if (result) {
            result =
                transaction->file_count == 1U &&
                transaction->files[0].replacement_text != NULL &&
                strcmp(
                    transaction->files[0].replacement_text,
                    expected_text) == 0;
        }

        edit_txn_dispose(transaction);
    }

    free(transaction);
    free(operations);
    return result;
}
