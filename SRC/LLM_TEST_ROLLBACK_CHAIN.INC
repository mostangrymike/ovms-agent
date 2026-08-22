{
    FILE *target_file;
    FILE *operation_test_file;
    int operation_test_ok;
    int target_created;
    int operation_test_created;
    const char *target_path =
        "OVMS_AGENT_TEST_ROLLBACK_CHAIN_TARGET.TMP";
    const char *operation_test_path =
        "OVMS_AGENT_TEST_ROLLBACK_CHAIN_OPS.TMP";

    operation_test_ok = 0;
    target_created = 0;
    operation_test_created = 0;
    target_file = NULL;
    operation_test_file = NULL;

    while (remove(operation_test_path) == 0) {
    }

    while (remove(target_path) == 0) {
    }

    if (!openai_selftest_file_exists(target_path) &&
        !openai_selftest_file_exists(operation_test_path)) {
        target_file = fopen(target_path, "w");

        if (target_file != NULL) {
            target_created = 1;
        }
    }

    if (target_file != NULL) {
        if (fputs("A", target_file) != EOF &&
            fclose(target_file) == 0) {
            target_file = NULL;
            operation_test_file =
                fopen(operation_test_path, "w");

            if (operation_test_file != NULL) {
                operation_test_created = 1;
            }
        }
    }

    if (operation_test_file != NULL) {
        if (fprintf(operation_test_file,
                    "operation_count=2\n"
                    "BEGIN_OPERATION\n"
                    "type=replace_text\n"
                    "path=%s\n"
                    "old_text=A\n"
                    "new_text=B\n"
                    "END_OPERATION\n"
                    "BEGIN_OPERATION\n"
                    "type=replace_text\n"
                    "path=%s\n"
                    "old_text=B\n"
                    "new_text=C\n"
                    "END_OPERATION\n",
                    target_path,
                    target_path) >= 0 &&
            fclose(operation_test_file) == 0) {
            operation_test_file = NULL;
            operation_test_ok =
                openai_exec_rollback_validate(
                    operation_test_path,
                    target_path,
                    "A",
                    2);
        }
    }

    if (target_file != NULL) {
        (void)fclose(target_file);
    }

    if (operation_test_file != NULL) {
        (void)fclose(operation_test_file);
    }

    if (operation_test_created) {
        while (remove(operation_test_path) == 0) {
        }
    }

    if (target_created) {
        while (remove(target_path) == 0) {
        }
    }

    openai_selftest_report(
        "failed build restores chained file",
        operation_test_ok,
        &passed_count,
        &failed_count
    );
}
