static int llm_m150b_write(
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

static int llm_m150b_absent(
    const char *path)
{
    FILE *file;

    file = fopen(path, "r");

    if (file == NULL) {
        return 1;
    }

    (void)fclose(file);
    return 0;
}

static int llm_m150b_read_equal(
    const char *path,
    const char *expected)
{
    char *text;
    int result;

    text = llm_read_text_file(path);
    result =
        text != NULL &&
        strcmp(text, expected) == 0;

    free(text);
    return result;
}

static int llm_m150b_stage(
    const char *plan_path,
    edit_txn *transaction)
{
    llm_saved_operation *operations;
    unsigned int operation_count;
    int result;

    operations = (llm_saved_operation *)calloc(
        LLM_EXECUTE_MAX_OPERATIONS,
        sizeof(*operations));

    if (operations == NULL) {
        return 0;
    }

    operation_count = 0U;
    result = execute_parse_operations(
        plan_path,
        operations,
        LLM_EXECUTE_MAX_OPERATIONS,
        &operation_count);

    if (result) {
        edit_txn_init(transaction);
        result = execute_stage_operations_chained(
            transaction,
            operations,
            operation_count);
    }

    free(operations);
    return result;
}

int llm_m150b_create_commit(void)
{
    const char *plan = "OVMS_AGENT_TEST_M150B_COMMIT.PLAN";
    const char *target = "OVMS_AGENT_TEST_M150B_COMMIT.TMP";
    edit_txn transaction;
    int result;

    while (remove(plan) == 0) {
    }
    while (remove(target) == 0) {
    }

    if (!llm_m150b_write(
            plan,
            "operation_count=1\n"
            "BEGIN_OPERATION\n"
            "type=create_file\n"
            "path=OVMS_AGENT_TEST_M150B_COMMIT.TMP\n"
            "BEGIN_NEW_TEXT\n"
            "created and committed\n"
            "END_NEW_TEXT\n"
            "END_OPERATION\n")) {
        return 0;
    }

    result =
        llm_m150b_stage(plan, &transaction) &&
        edit_txn_write(&transaction) &&
        edit_txn_commit(&transaction) &&
        llm_m150b_read_equal(
            target,
            "created and committed");

    edit_txn_dispose(&transaction);

    while (remove(plan) == 0) {
    }
    while (remove(target) == 0) {
    }

    return result;
}

int llm_m150b_create_rollback(void)
{
    const char *plan = "OVMS_AGENT_TEST_M150B_ROLL.PLAN";
    const char *target = "OVMS_AGENT_TEST_M150B_ROLL.TMP";
    edit_txn transaction;
    int result;

    while (remove(plan) == 0) {
    }
    while (remove(target) == 0) {
    }

    if (!llm_m150b_write(
            plan,
            "operation_count=1\n"
            "BEGIN_OPERATION\n"
            "type=create_file\n"
            "path=OVMS_AGENT_TEST_M150B_ROLL.TMP\n"
            "BEGIN_NEW_TEXT\n"
            "created then rolled back\n"
            "END_NEW_TEXT\n"
            "END_OPERATION\n")) {
        return 0;
    }

    result =
        llm_m150b_stage(plan, &transaction) &&
        edit_txn_write(&transaction) &&
        edit_txn_rollback(&transaction) &&
        llm_m150b_absent(target);

    edit_txn_dispose(&transaction);

    while (remove(plan) == 0) {
    }
    while (remove(target) == 0) {
    }

    return result;
}

int llm_m150b_existing_reject(void)
{
    const char *plan = "OVMS_AGENT_TEST_M150B_EXIST.PLAN";
    const char *target = "OVMS_AGENT_TEST_M150B_EXIST.TMP";
    edit_txn transaction;
    int stage_result;
    int result;

    while (remove(plan) == 0) {
    }
    while (remove(target) == 0) {
    }

    if (!llm_m150b_write(target, "original\n") ||
        !llm_m150b_write(
            plan,
            "operation_count=1\n"
            "BEGIN_OPERATION\n"
            "type=create_file\n"
            "path=OVMS_AGENT_TEST_M150B_EXIST.TMP\n"
            "BEGIN_NEW_TEXT\n"
            "must not replace\n"
            "END_NEW_TEXT\n"
            "END_OPERATION\n")) {
        return 0;
    }

    stage_result = llm_m150b_stage(plan, &transaction);

    if (stage_result) {
        edit_txn_dispose(&transaction);
    }

    result =
        !stage_result &&
        llm_m150b_read_equal(target, "original\n");

    while (remove(plan) == 0) {
    }
    while (remove(target) == 0) {
    }

    return result;
}

int llm_m150b_mixed_rollback(void)
{
    const char *plan = "OVMS_AGENT_TEST_M150B_MIX.PLAN";
    const char *existing = "OVMS_AGENT_TEST_M150B_MIX_A.TMP";
    const char *created = "OVMS_AGENT_TEST_M150B_MIX_B.TMP";
    edit_txn transaction;
    int result;

    while (remove(plan) == 0) {
    }
    while (remove(existing) == 0) {
    }
    while (remove(created) == 0) {
    }

    if (!llm_m150b_write(existing, "old value\n") ||
        !llm_m150b_write(
            plan,
            "operation_count=2\n"
            "BEGIN_OPERATION\n"
            "type=replace_text\n"
            "path=OVMS_AGENT_TEST_M150B_MIX_A.TMP\n"
            "old_text=old value\n"
            "new_text=new value\n"
            "END_OPERATION\n"
            "BEGIN_OPERATION\n"
            "type=create_file\n"
            "path=OVMS_AGENT_TEST_M150B_MIX_B.TMP\n"
            "BEGIN_NEW_TEXT\n"
            "new file\n"
            "END_NEW_TEXT\n"
            "END_OPERATION\n")) {
        return 0;
    }

    result =
        llm_m150b_stage(plan, &transaction) &&
        edit_txn_write(&transaction) &&
        edit_txn_rollback(&transaction) &&
        llm_m150b_read_equal(existing, "old value\n") &&
        llm_m150b_absent(created);

    edit_txn_dispose(&transaction);

    while (remove(plan) == 0) {
    }
    while (remove(existing) == 0) {
    }
    while (remove(created) == 0) {
    }

    return result;
}

int llm_m150b_same_path_reject(void)
{
    const char *plan = "OVMS_AGENT_TEST_M150B_SAME.PLAN";
    const char *target = "OVMS_AGENT_TEST_M150B_SAME.TMP";
    edit_txn transaction;
    int stage_result;

    while (remove(plan) == 0) {
    }
    while (remove(target) == 0) {
    }

    if (!llm_m150b_write(
            plan,
            "operation_count=2\n"
            "BEGIN_OPERATION\n"
            "type=create_file\n"
            "path=OVMS_AGENT_TEST_M150B_SAME.TMP\n"
            "BEGIN_NEW_TEXT\n"
            "first\n"
            "END_NEW_TEXT\n"
            "END_OPERATION\n"
            "BEGIN_OPERATION\n"
            "type=create_file\n"
            "path=OVMS_AGENT_TEST_M150B_SAME.TMP\n"
            "BEGIN_NEW_TEXT\n"
            "second\n"
            "END_NEW_TEXT\n"
            "END_OPERATION\n")) {
        return 0;
    }

    stage_result = llm_m150b_stage(plan, &transaction);

    if (stage_result) {
        edit_txn_dispose(&transaction);
    }

    while (remove(plan) == 0) {
    }
    while (remove(target) == 0) {
    }

    return !stage_result;
}
