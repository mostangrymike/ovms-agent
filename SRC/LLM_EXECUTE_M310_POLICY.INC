int llm_set_approval(const char *arguments);
static int execute_plan_recover(void);
static int execute_mark_consumed_safe(void);
void llm_m310_execute_legacy(agent_state *state);

static int llm_m310_build_policy(const char *path)
{
    FILE *file;
    char line[256];
    int policy;
    int saw_policy;

    if (path == NULL || *path == '\0') {
        return -1;
    }

    file = fopen(path, "r");
    if (file == NULL) {
        return -1;
    }

    policy = 1;
    saw_policy = 0;

    while (fgets(line, sizeof(line), file) != NULL) {
        size_t length;

        length = strlen(line);
        while (length > 0U &&
               (line[length - 1U] == '\n' ||
                line[length - 1U] == '\r')) {
            line[--length] = '\0';
        }

        if (strncmp(line, "operation_count=", 16U) == 0) {
            break;
        }

        if (strncmp(line, "build=", 6U) != 0) {
            continue;
        }

        if (saw_policy) {
            (void)fclose(file);
            return -1;
        }

        saw_policy = 1;

        if (strcmp(line, "build=required") == 0) {
            policy = 1;
        } else if (strcmp(line, "build=skip") == 0) {
            policy = 0;
        } else {
            (void)fclose(file);
            return -1;
        }
    }

    (void)fclose(file);
    return policy;
}

static void llm_m310_execute_skip(agent_state *state)
{
    llm_saved_operation *operations;
    edit_txn *transaction;
    const char *approval_name;
    unsigned int operation_count;
    unsigned int index;
    int write_ok;
    int rollback_ok;

    llm_log_event("AGENT/EXECUTE", "start", 0);

    if (state == NULL ||
        state->project_root == NULL ||
        *state->project_root == '\0') {
        (void)puts("OVMS_AGENT_ROOT is not defined.");
        return;
    }

    if (!execute_plan_recover()) {
        llm_plan_recovery_sync(LLM_PLAN_FILE);
        (void)puts("Unable to recover plan-consumption artifacts.");
        llm_log_event(
            "AGENT/EXECUTE",
            "plan_recovery_failed",
            0);
        return;
    }

    llm_plan_recovery_sync(LLM_PLAN_FILE);

    if (!llm_plan_is_current(1)) {
        (void)puts(
            "Saved plan execution refused. Run AGENT/PLAN again.");
        llm_log_event(
            "AGENT/EXECUTE",
            "plan_invalid",
            0);
        return;
    }

    approval_name = llm_approval_name();
    if (approval_name == NULL ||
        (strcmp(approval_name, "workspace") != 0 &&
         strcmp(approval_name, "full") != 0 &&
         strcmp(approval_name, "autopilot") != 0)) {
        llm_plan_approval_clear();
        (void)puts(
            "Saved plan execution refused by current approval policy.");
        llm_log_event(
            "AGENT/EXECUTE",
            "policy_refused",
            0);
        return;
    }

    if (!state->write_enabled) {
        (void)puts(
            "Saved plan execution refused because guarded writes are disabled.");
        llm_log_event(
            "AGENT/EXECUTE",
            "write_gate",
            0);
        return;
    }

    operations = (llm_saved_operation *)calloc(
        LLM_EXECUTE_MAX_OPERATIONS,
        sizeof(*operations));
    transaction = (edit_txn *)malloc(sizeof(*transaction));

    if (operations == NULL || transaction == NULL) {
        (void)puts("Insufficient memory for plan-wide transaction.");
        free(operations);
        free(transaction);
        llm_log_event(
            "AGENT/EXECUTE",
            "allocation_failed",
            0);
        return;
    }

    operation_count = 0U;
    if (!execute_parse_operations(
            LLM_PLAN_FILE,
            operations,
            LLM_EXECUTE_MAX_OPERATIONS,
            &operation_count)) {
        (void)puts(
            "Saved plan execution refused because no deterministic "
            "operations are available.");
        free(operations);
        free(transaction);
        llm_log_event(
            "AGENT/EXECUTE",
            "operation_invalid",
            0);
        return;
    }

    (void)puts("Parsed saved operations:");
    for (index = 0U; index < operation_count; ++index) {
        (void)printf(
            "  %u: %s\n",
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
        llm_log_event(
            "AGENT/EXECUTE",
            "stage_failed",
            0);
        return;
    }

    if (!llm_plan_approval_valid(LLM_PLAN_FILE)) {
        if (llm_approval_invalidated) {
            (void)puts(
                "Saved plan approval was invalidated; run AGENT/APPROVE again.");
        } else {
            (void)puts(
                "Saved plan execution requires AGENT/APPROVE.");
        }
        edit_txn_dispose(transaction);
        free(transaction);
        free(operations);
        llm_log_event(
            "AGENT/EXECUTE",
            "approval_required",
            0);
        return;
    }

    llm_plan_approval_consume();

    (void)printf(
        "Executing %u saved operations as one transaction.\n",
        operation_count);

    write_ok = edit_txn_write(transaction);
    if (!write_ok) {
        rollback_ok = edit_txn_rollback(transaction);
        (void)printf(
            "Plan-wide write failed. Transaction rollback: %s\n",
            rollback_ok ? "PASS" : "FAIL");
        edit_txn_dispose(transaction);
        free(transaction);
        free(operations);
        llm_log_event(
            "AGENT/EXECUTE",
            "write_failed",
            rollback_ok);
        return;
    }

    llm_log_event(
        "AGENT/EXECUTE",
        "transaction_written",
        (int)operation_count);

    if (!edit_txn_commit(transaction)) {
        (void)puts(
            "Transaction commit failed after build-skipped execution.");
        (void)edit_txn_rollback(transaction);
        edit_txn_dispose(transaction);
        free(transaction);
        free(operations);
        llm_log_event(
            "AGENT/EXECUTE",
            "commit_failed",
            0);
        return;
    }

    llm_last_workflow = LLM_WORKFLOW_EXECUTE;
    llm_last_build_known = 0;
    llm_last_build_status = 0;
    llm_last_rollback = LLM_ROLLBACK_NONE;
    llm_state_save();

    (void)puts("Controlled build skipped by saved plan.");
    llm_log_event(
        "AGENT/EXECUTE",
        "build_skipped",
        0);

    edit_txn_dispose(transaction);
    free(transaction);
    free(operations);

    llm_plan_approval_clear();

    if (execute_mark_consumed_safe()) {
        (void)puts("Saved plan marked consumed.");
        llm_log_event(
            "AGENT/EXECUTE",
            "plan_consumed",
            1);
    } else {
        (void)puts(
            "Warning: changes committed, but plan could not be marked consumed.");
        llm_log_event(
            "AGENT/EXECUTE",
            "consume_failed",
            0);
    }

    (void)puts("Plan-wide transaction completed successfully.");
}

void llm_plan_execute(agent_state *state)
{
    int build_policy;

    build_policy = llm_m310_build_policy(LLM_PLAN_FILE);

    if (build_policy < 0) {
        (void)puts(
            "Saved plan build policy is invalid; regenerate it with AGENT/PLAN.");
        llm_log_event(
            "AGENT/EXECUTE",
            "build_policy_invalid",
            0);
        return;
    }

    if (build_policy == 0) {
        llm_m310_execute_skip(state);
        return;
    }

    llm_m310_execute_legacy(state);
}

#define llm_plan_execute llm_m310_execute_legacy
