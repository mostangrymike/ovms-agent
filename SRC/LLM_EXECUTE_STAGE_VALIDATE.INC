int llm_exec_stage_ops_file(
    const char *path,
    const char *expected_text)
{
    llm_saved_operation *operations;
    edit_txn *transaction;
    unsigned int operation_count;
    int result;

    if (path == NULL ||
        *path == '\0' ||
        expected_text == NULL) {
        return 0;
    }

    operations = (llm_saved_operation *)calloc(
        LLM_EXECUTE_MAX_OPERATIONS,
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
        LLM_EXECUTE_MAX_OPERATIONS,
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

int llm_m252_delete_exec(
    const char *path,
    int commit_change)
{
    llm_saved_operation *operations;
    edit_txn *transaction;
    unsigned int operation_count;
    int result;

    if (path == NULL || *path == '\0') {
        return 0;
    }

    operations = (llm_saved_operation *)calloc(
        LLM_EXECUTE_MAX_OPERATIONS,
        sizeof(*operations));
    transaction = (edit_txn *)malloc(sizeof(*transaction));

    if (operations == NULL || transaction == NULL) {
        free(transaction);
        free(operations);
        return 0;
    }

    operation_count = 0U;
    result = execute_parse_operations(
        path,
        operations,
        LLM_EXECUTE_MAX_OPERATIONS,
        &operation_count);

    if (result) {
        edit_txn_init(transaction);
        result =
            operation_count == 1U &&
            operations[0].is_block == 2 &&
            execute_stage_operations_chained(
                transaction,
                operations,
                operation_count) &&
            edit_txn_write(transaction);

        if (result) {
            if (commit_change) {
                result = edit_txn_commit(transaction);
            } else {
                result = edit_txn_rollback(transaction);
            }
        }

        edit_txn_dispose(transaction);
    }

    free(transaction);
    free(operations);
    return result;
}

int llm_m252_rename_exec(
    const char *path,
    int commit_change)
{
    llm_saved_operation *operations;
    edit_txn *transaction;
    unsigned int operation_count;
    int result;

    if (path == NULL || *path == '\0') {
        return 0;
    }

    operations = (llm_saved_operation *)calloc(
        LLM_EXECUTE_MAX_OPERATIONS,
        sizeof(*operations));
    transaction = (edit_txn *)malloc(sizeof(*transaction));

    if (operations == NULL || transaction == NULL) {
        free(transaction);
        free(operations);
        return 0;
    }

    operation_count = 0U;
    result = execute_parse_operations(
        path,
        operations,
        LLM_EXECUTE_MAX_OPERATIONS,
        &operation_count);

    if (result) {
        edit_txn_init(transaction);
        result =
            operation_count == 1U &&
            operations[0].is_block == 3 &&
            execute_stage_operations_chained(
                transaction,
                operations,
                operation_count) &&
            edit_txn_write(transaction);

        if (result) {
            if (commit_change) {
                result = edit_txn_commit(transaction);
            } else {
                result = edit_txn_rollback(transaction);
            }
        }

        edit_txn_dispose(transaction);
    }

    free(transaction);
    free(operations);
    return result;
}

int llm_m252_move_exec(
    const char *path,
    int commit_change)
{
    llm_saved_operation *operations;
    edit_txn *transaction;
    unsigned int operation_count;
    int result;

    if (path == NULL || *path == '\0') {
        return 0;
    }

    operations = (llm_saved_operation *)calloc(
        LLM_EXECUTE_MAX_OPERATIONS,
        sizeof(*operations));
    transaction = (edit_txn *)malloc(sizeof(*transaction));

    if (operations == NULL || transaction == NULL) {
        free(transaction);
        free(operations);
        return 0;
    }

    operation_count = 0U;
    result = execute_parse_operations(
        path,
        operations,
        LLM_EXECUTE_MAX_OPERATIONS,
        &operation_count);

    if (result) {
        edit_txn_init(transaction);
        result =
            operation_count == 1U &&
            operations[0].is_block == 4 &&
            execute_stage_operations_chained(
                transaction,
                operations,
                operation_count) &&
            edit_txn_write(transaction);

        if (result) {
            if (commit_change) {
                result = edit_txn_commit(transaction);
            } else {
                result = edit_txn_rollback(transaction);
            }
        }

        edit_txn_dispose(transaction);
    }

    free(transaction);
    free(operations);
    return result;
}
