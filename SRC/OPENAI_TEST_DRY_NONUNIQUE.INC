{
    FILE *target_file;
    FILE *operation_test_file;
    int operation_test_ok;
    int target_created;
    int operation_test_created;
    const char *target_path =
        "OVMS_AGENT_TEST_DRY_NONUNIQUE_TARGET.TMP";
    const char *operation_test_path =
        "OVMS_AGENT_TEST_DRY_NONUNIQUE_PLAN.TMP";

    operation_test_ok = 0;
    target_created = 0;
    operation_test_created = 0;
    target_file = NULL;
    operation_test_file = NULL;

    if (!openai_selftest_file_exists(target_path) &&
        !openai_selftest_file_exists(operation_test_path)) {
        target_file = fopen(target_path, "w");

        if (target_file != NULL) {
            target_created = 1;
        }
    }

    if (target_file != NULL) {
        if (fputs("A A", target_file) != EOF &&
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
                    "operation_count=1\n"
                    "BEGIN_OPERATION\n"
                    "type=replace_block\n"
                    "path=%s\n"
                    "BEGIN_OLD_TEXT\n"
                    "A\n"
                    "END_OLD_TEXT\n"
                    "BEGIN_NEW_TEXT\n"
                    "B\n"
                    "END_NEW_TEXT\n"
                    "END_OPERATION\n",
                    target_path) >= 0 &&
            fclose(operation_test_file) == 0) {
            operation_test_file = NULL;
            operation_test_ok =
                openai_exec_stage_expect_fail(
                    operation_test_path,
                    target_path,
                    "A A");
        }
    }

    if (target_file != NULL) {
        (void)fclose(target_file);
    }

    if (operation_test_file != NULL) {
        (void)fclose(operation_test_file);
    }

    if (operation_test_created) {
        (void)remove(operation_test_path);
    }

    if (target_created) {
        (void)remove(target_path);
    }

    openai_selftest_report(
        "dry run rejects non-unique old_text",
        operation_test_ok,
        &passed_count,
        &failed_count
    );
}
