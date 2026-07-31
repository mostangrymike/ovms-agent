{
    FILE *operation_test_file;
    int operation_test_ok;
    int operation_test_created;
    const char *operation_test_path =
        "OVMS_AGENT_TEST_BLOCK_MIXED_LEGACY.TMP";

    operation_test_ok = 0;
    operation_test_created = 0;
    operation_test_file = NULL;

    if (!openai_selftest_file_exists(operation_test_path)) {
        operation_test_file = fopen(operation_test_path, "w");
        if (operation_test_file != NULL) {
            operation_test_created = 1;
        }
    }

    if (operation_test_file != NULL) {
        if (fprintf(operation_test_file,
                    "operation_count=1\n"
                    "BEGIN_OPERATION\n"
                    "type=replace_block\n"
                    "path=SRC/openai_status.c\n"
                    "old_text=legacy text\n"
                    "BEGIN_NEW_TEXT\n"
                    "new text\n"
                    "END_NEW_TEXT\n"
                    "END_OPERATION\n") >= 0 &&
            fclose(operation_test_file) == 0) {
            operation_test_file = NULL;
            operation_test_ok =
                !openai_exec_validate_ops_file(operation_test_path);
        }
    }

    if (operation_test_file != NULL) {
        (void)fclose(operation_test_file);
    }

    if (operation_test_created) {
        (void)remove(operation_test_path);
    }

    openai_selftest_report(
        "mixed block and legacy fields rejected",
        operation_test_ok,
        &passed_count,
        &failed_count
    );
}
