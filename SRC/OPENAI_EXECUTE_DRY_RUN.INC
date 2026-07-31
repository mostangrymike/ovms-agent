void openai_plan_execute_dry(agent_state *state)
{
    openai_saved_operation *operations;
    edit_txn *transaction;
    unsigned int operation_count;
    unsigned int index;

    openai_log_event("AGENT/EXECUTE/DRY_RUN", "start", 0);

    if (state == NULL ||
        state->project_root == NULL ||
        *state->project_root == '\0') {
        (void)puts("OVMS_AGENT_ROOT is not defined.");
        return;
    }

    if (!openai_plan_is_current(1)) {
        (void)puts("Saved plan dry run refused. Run AGENT/PLAN again.");
        openai_log_event("AGENT/EXECUTE/DRY_RUN", "plan_invalid", 0);
        return;
    }

    operations = (openai_saved_operation *)calloc(
        OPENAI_EXECUTE_MAX_OPERATIONS,
        sizeof(*operations));
    transaction = (edit_txn *)malloc(sizeof(*transaction));

    if (operations == NULL || transaction == NULL) {
        (void)puts("Insufficient memory for execution dry run.");
        free(transaction);
        free(operations);
        openai_log_event("AGENT/EXECUTE/DRY_RUN", "allocation_failed", 0);
        return;
    }

    operation_count = 0U;

    if (!execute_parse_operations(
            OPENAI_PLAN_FILE,
            operations,
            OPENAI_EXECUTE_MAX_OPERATIONS,
            &operation_count)) {
        (void)puts(
            "Saved plan dry run refused because no deterministic "
            "operations are available.");
        free(transaction);
        free(operations);
        openai_log_event("AGENT/EXECUTE/DRY_RUN", "operation_invalid", 0);
        return;
    }

    (void)puts("Parsed saved operations:");

    for (index = 0U; index < operation_count; ++index) {
        (void)printf("  %u: %s\n",
                     index + 1U,
                     operations[index].path);
    }

    edit_txn_init(transaction);

    if (!execute_stage_operations_chained(
            transaction,
            operations,
            operation_count)) {
        edit_txn_dispose(transaction);
        free(transaction);
        free(operations);
        openai_log_event("AGENT/EXECUTE/DRY_RUN", "stage_failed", 0);
        (void)puts("Execution dry run failed during in-memory staging.");
        return;
    }

    (void)printf(
        "Preflight staged %u unique file%s successfully.\n",
        transaction->file_count,
        transaction->file_count == 1U ? "" : "s");

    for (index = 0U; index < transaction->file_count; ++index) {
        (void)printf("  staged: %s\n",
                     transaction->files[index].path);
    }

    edit_txn_dispose(transaction);
    free(transaction);
    free(operations);

    openai_log_event(
        "AGENT/EXECUTE/DRY_RUN",
        "success",
        (int)operation_count);

    (void)puts("No files were written. No build was run.");
    (void)puts("Saved plan remains active.");
}
