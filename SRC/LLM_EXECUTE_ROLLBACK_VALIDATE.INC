int llm_exec_rollback_validate(
    const char *operations_path,
    const char *target_path,
    const char *expected_original_text,
    int simulated_build_status)
{
    llm_saved_operation *operations;
    edit_txn *transaction;
    char *actual_text;
    unsigned int operation_count;
    int result;
    int rollback_ok;

    if (operations_path == NULL ||
        *operations_path == '\0' ||
        target_path == NULL ||
        *target_path == '\0' ||
        expected_original_text == NULL ||
        (simulated_build_status & 1) != 0) {
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
        operations_path,
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
            result = edit_txn_write(transaction);
        }

        if (result) {
            rollback_ok = edit_txn_rollback(transaction);
            result = rollback_ok;
        }

        edit_txn_dispose(transaction);
    }

    actual_text = llm_read_text_file(target_path);

    if (actual_text == NULL ||
        strcmp(actual_text, expected_original_text) != 0) {
        result = 0;
    }

    free(actual_text);
    free(transaction);
    free(operations);
    return result;
}
