static int llm_m150d_write(
    const char *path,
    const char *text)
{
    FILE *file;
    int result;

    file = fopen(path, "w");

    if (file == NULL) {
        return 0;
    }

    result = fputs(text, file) != EOF;

    if (fclose(file) != 0) {
        result = 0;
    }

    return result;
}

static int llm_m150d_exists(
    const char *path)
{
    FILE *file;

    file = fopen(path, "r");

    if (file == NULL) {
        return 0;
    }

    (void)fclose(file);
    return 1;
}

static int llm_m150d_contains(
    const char *path,
    const char *text)
{
    FILE *file;
    char line[256];

    file = fopen(path, "r");

    if (file == NULL) {
        return 0;
    }

    while (fgets(line, sizeof(line), file) != NULL) {
        if (strstr(line, text) != NULL) {
            (void)fclose(file);
            return 1;
        }
    }

    (void)fclose(file);
    return 0;
}

static void llm_m150d_remove_pair(
    const char *path)
{
    char checksum_path[512];
    int written;

    while (remove(path) == 0) {
    }

    written = snprintf(
        checksum_path,
        sizeof(checksum_path),
        "%s.CHK",
        path);

    if (written >= 0 &&
        (size_t)written < sizeof(checksum_path)) {
        while (remove(checksum_path) == 0) {
        }
    }
}

static int llm_m150d_make_plan(
    const char *plan_path,
    const char *target_path)
{
    FILE *file;
    int result;

    file = fopen(plan_path, "w");

    if (file == NULL) {
        return 0;
    }

    result =
        fprintf(
            file,
            "format=1\n"
            "status=active\n"
            "file_count=0\n"
            "\n"
            "[plan]\n"
            "Goal\n"
            "Create one validation file.\n"
            "\n"
            "Files to inspect\n"
            "None.\n"
            "\n"
            "Files to modify\n"
            "None.\n"
            "\n"
            "Files to create\n"
            "%s\n"
            "\n"
            "Ordered edits\n"
            "Create the validation file.\n"
            "\n"
            "Validation\n"
            "$ @BUILD\n"
            "\n"
            "Risks\n"
            "Validation-only fixture.\n"
            "\n"
            "Authority required\n"
            "Plan approval and transactional execution.\n"
            "\n"
            "operation_count=1\n"
            "BEGIN_OPERATION\n"
            "type=create_file\n"
            "path=%s\n"
            "BEGIN_NEW_TEXT\n"
            "M150D created content\n"
            "END_NEW_TEXT\n"
            "END_OPERATION\n",
            target_path,
            target_path) >= 0;

    if (fclose(file) != 0) {
        result = 0;
    }

    return
        result &&
        llm_plan_checksum_write(plan_path);
}

static int llm_m150d_run(
    const char *plan_path,
    const char *new_path,
    const char *backup_path,
    int build_status)
{
    llm_saved_operation *operations;
    edit_txn *transaction;
    unsigned int operation_count;
    int result;
    int rollback_ok;

    operations = (llm_saved_operation *)calloc(
        LLM_EXECUTE_MAX_OPERATIONS,
        sizeof(*operations));

    transaction = (edit_txn *)malloc(
        sizeof(*transaction));

    if (operations == NULL ||
        transaction == NULL) {
        free(transaction);
        free(operations);
        return 0;
    }

    operation_count = 0U;

    llm_plan_recovery_sync(plan_path);

    result =
        llm_plan_file_current(plan_path, 0) &&
        execute_parse_operations(
            plan_path,
            operations,
            LLM_EXECUTE_MAX_OPERATIONS,
            &operation_count);

    if (!result) {
        free(transaction);
        free(operations);
        return 0;
    }

    edit_txn_init(transaction);

    result = execute_stage_operations_chained(
        transaction,
        operations,
        operation_count);

    if (!result ||
        !llm_plan_approval_valid(plan_path)) {
        edit_txn_dispose(transaction);
        free(transaction);
        free(operations);
        return 0;
    }

    llm_plan_approval_consume();

    result = edit_txn_write(transaction);

    if (!result) {
        rollback_ok = edit_txn_rollback(transaction);
        edit_txn_dispose(transaction);
        free(transaction);
        free(operations);
        return rollback_ok ? 0 : 0;
    }

    if ((build_status & 1) == 0) {
        rollback_ok = edit_txn_rollback(transaction);
        edit_txn_dispose(transaction);
        free(transaction);
        free(operations);
        return rollback_ok ? -1 : 0;
    }

    if (!edit_txn_commit(transaction)) {
        (void)edit_txn_rollback(transaction);
        edit_txn_dispose(transaction);
        free(transaction);
        free(operations);
        return 0;
    }

    edit_txn_dispose(transaction);
    free(transaction);
    free(operations);

    llm_plan_approval_clear();

    if (!execute_mark_consumed_file(
            plan_path,
            new_path,
            backup_path,
            0)) {
        return 0;
    }

    return 1;
}

static int llm_m150d_setup(
    const char *plan,
    const char *target,
    const char *new_path,
    const char *backup_path)
{
    llm_m150d_remove_pair(plan);

    while (remove(target) == 0) {
    }

    llm_m150d_remove_pair(new_path);
    llm_m150d_remove_pair(backup_path);
    llm_plan_approval_clear();

    return
        llm_m150d_make_plan(plan, target) &&
        llm_plan_approve_file(plan) &&
        llm_plan_approval_valid(plan);
}

static void llm_m150d_cleanup(
    const char *plan,
    const char *target,
    const char *new_path,
    const char *backup_path)
{
    llm_m150d_remove_pair(plan);

    while (remove(target) == 0) {
    }

    llm_m150d_remove_pair(new_path);
    llm_m150d_remove_pair(backup_path);
    llm_plan_approval_clear();
}

int llm_m150d_success(void)
{
    const char *plan =
        "OVMS_AGENT_TEST_M150D_OK.PLAN";
    const char *target =
        "OVMS_AGENT_TEST_M150D_OK.TMP";
    const char *new_path =
        "OVMS_AGENT_TEST_M150D_OK.NEW";
    const char *backup_path =
        "OVMS_AGENT_TEST_M150D_OK.OLD";
    int result;

    if (!llm_m150d_setup(
            plan,
            target,
            new_path,
            backup_path)) {
        llm_m150d_cleanup(
            plan,
            target,
            new_path,
            backup_path);
        return 0;
    }

    result =
        llm_m150d_run(
            plan,
            new_path,
            backup_path,
            1) == 1 &&
        llm_m150d_exists(target) &&
        llm_m150d_contains(
            target,
            "M150D created content") &&
        llm_m150d_contains(
            plan,
            "status=consumed") &&
        !llm_plan_file_current(plan, 0) &&
        !llm_plan_approved &&
        llm_approved_hash == 0UL;

    llm_m150d_cleanup(
        plan,
        target,
        new_path,
        backup_path);

    return result;
}

int llm_m150d_rollback(void)
{
    const char *plan =
        "OVMS_AGENT_TEST_M150D_FAIL.PLAN";
    const char *target =
        "OVMS_AGENT_TEST_M150D_FAIL.TMP";
    const char *new_path =
        "OVMS_AGENT_TEST_M150D_FAIL.NEW";
    const char *backup_path =
        "OVMS_AGENT_TEST_M150D_FAIL.OLD";
    int result;

    if (!llm_m150d_setup(
            plan,
            target,
            new_path,
            backup_path)) {
        llm_m150d_cleanup(
            plan,
            target,
            new_path,
            backup_path);
        return 0;
    }

    result =
        llm_m150d_run(
            plan,
            new_path,
            backup_path,
            0) == -1 &&
        !llm_m150d_exists(target) &&
        llm_m150d_contains(
            plan,
            "status=active") &&
        llm_plan_file_current(plan, 0) &&
        !llm_plan_approved &&
        llm_approved_hash == 0UL;

    llm_m150d_cleanup(
        plan,
        target,
        new_path,
        backup_path);

    return result;
}

int llm_m150d_tamper(void)
{
    const char *plan =
        "OVMS_AGENT_TEST_M150D_BAD.PLAN";
    const char *target =
        "OVMS_AGENT_TEST_M150D_BAD.TMP";
    const char *new_path =
        "OVMS_AGENT_TEST_M150D_BAD.NEW";
    const char *backup_path =
        "OVMS_AGENT_TEST_M150D_BAD.OLD";
    FILE *file;
    int result;

    if (!llm_m150d_setup(
            plan,
            target,
            new_path,
            backup_path)) {
        llm_m150d_cleanup(
            plan,
            target,
            new_path,
            backup_path);
        return 0;
    }

    file = fopen(plan, "a");

    if (file == NULL) {
        llm_m150d_cleanup(
            plan,
            target,
            new_path,
            backup_path);
        return 0;
    }

    if (fputs("tampered=1\n", file) == EOF ||
        fclose(file) != 0) {
        llm_m150d_cleanup(
            plan,
            target,
            new_path,
            backup_path);
        return 0;
    }

    result =
        llm_m150d_run(
            plan,
            new_path,
            backup_path,
            1) == 0 &&
        !llm_m150d_exists(target) &&
        !llm_plan_approved &&
        llm_approval_invalidated;

    llm_m150d_cleanup(
        plan,
        target,
        new_path,
        backup_path);

    return result;
}
