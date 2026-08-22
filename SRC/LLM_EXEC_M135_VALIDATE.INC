static int llm_m135_write_text(
    const char *path,
    const char *text)
{
    FILE *file;

    if (path == NULL || text == NULL) {
        return 0;
    }

    file = fopen(path, "w");

    if (file == NULL) {
        return 0;
    }

    if (fputs(text, file) == EOF) {
        (void)fclose(file);
        return 0;
    }

    return fclose(file) == 0;
}

static int llm_m135_text_equals(
    const char *path,
    const char *expected)
{
    char *text;
    int ok;

    text = llm_read_text_file(path);

    if (text == NULL) {
        return 0;
    }

    ok = strcmp(text, expected) == 0;
    free(text);
    return ok;
}

int llm_m135_shared_validation(
    const char *operations_path,
    const char *target_path)
{
    int validate_ok;
    int dry_ok;

    if (!llm_m135_write_text(
            target_path,
            "ALPHA\n")) {
        return 0;
    }

    if (!llm_m135_write_text(
            operations_path,
            "operation_count=1\n"
            "BEGIN_OPERATION\n"
            "type=unsupported\n"
            "path=OVMS_AGENT_TEST_M135_SHARED.TMP\n"
            "old_text=ALPHA\n"
            "new_text=BETA\n"
            "END_OPERATION\n")) {
        return 0;
    }

    validate_ok =
        !llm_exec_validate_ops_file(
            operations_path);

    dry_ok =
        !llm_exec_dry_validate(
            operations_path,
            target_path,
            "ALPHA\n");

    return
        validate_ok &&
        dry_ok &&
        llm_m135_text_equals(
            target_path,
            "ALPHA\n");
}

int llm_m135_prewrite_validation(
    const char *operations_path,
    const char *target_path)
{
    if (!llm_m135_write_text(
            target_path,
            "ALPHA\n")) {
        return 0;
    }

    if (!llm_m135_write_text(
            operations_path,
            "operation_count=2\n"
            "BEGIN_OPERATION\n"
            "type=replace_text\n"
            "path=OVMS_AGENT_TEST_M135_PREWRITE.TMP\n"
            "old_text=ALPHA\n"
            "new_text=BETA\n"
            "END_OPERATION\n"
            "BEGIN_OPERATION\n"
            "type=replace_text\n"
            "path=OVMS_AGENT_TEST_M135_PREWRITE.TMP\n"
            "old_text=MISSING\n"
            "new_text=GAMMA\n"
            "END_OPERATION\n")) {
        return 0;
    }

    return
        llm_exec_stage_expect_fail(
            operations_path,
            target_path,
            "ALPHA\n") &&
        llm_m135_text_equals(
            target_path,
            "ALPHA\n");
}

int llm_m135_chain_deterministic(
    const char *operations_path,
    const char *target_path)
{
    if (!llm_m135_write_text(
            target_path,
            "ALPHA\n")) {
        return 0;
    }

    if (!llm_m135_write_text(
            operations_path,
            "operation_count=2\n"
            "BEGIN_OPERATION\n"
            "type=replace_text\n"
            "path=OVMS_AGENT_TEST_M135_CHAIN.TMP\n"
            "old_text=ALPHA\n"
            "new_text=BETA\n"
            "END_OPERATION\n"
            "BEGIN_OPERATION\n"
            "type=replace_text\n"
            "path=OVMS_AGENT_TEST_M135_CHAIN.TMP\n"
            "old_text=BETA\n"
            "new_text=GAMMA\n"
            "END_OPERATION\n")) {
        return 0;
    }

    return
        llm_exec_stage_ops_file(
            operations_path,
            "GAMMA\n") &&
        llm_m135_text_equals(
            target_path,
            "ALPHA\n");
}

int llm_m135_rollback_all(
    const char *operations_path,
    const char *target_path)
{
    if (!llm_m135_write_text(
            target_path,
            "ALPHA\n")) {
        return 0;
    }

    if (!llm_m135_write_text(
            operations_path,
            "operation_count=1\n"
            "BEGIN_OPERATION\n"
            "type=replace_text\n"
            "path=OVMS_AGENT_TEST_M135_ROLLBACK.TMP\n"
            "old_text=ALPHA\n"
            "new_text=BETA\n"
            "END_OPERATION\n")) {
        return 0;
    }

    return
        llm_exec_rollback_validate(
            operations_path,
            target_path,
            "ALPHA\n",
            0) &&
        llm_m135_text_equals(
            target_path,
            "ALPHA\n");
}
