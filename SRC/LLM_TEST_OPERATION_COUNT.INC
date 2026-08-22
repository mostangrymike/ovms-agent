{
    FILE *operation_test_file;
    int operation_test_ok;
    int operation_test_created;
    const char *operation_test_path =
        "OVMS_AGENT_TEST_OPERATIONS.TMP";

    operation_test_ok = 0;
    operation_test_created = 0;
    operation_test_file = NULL;

    if (!llm_selftest_file_exists(operation_test_path)) {
        operation_test_file =
            fopen(operation_test_path, "w");

        if (operation_test_file != NULL) {
            operation_test_created = 1;
        }
    }

    if (operation_test_file != NULL) {
        if (fprintf(
                operation_test_file,
                "operation_count=2\n"
                "BEGIN_OPERATION\n"
                "type=replace_text\n"
                "path=SRC/llm_status.c\n"
                "old_text=old\n"
                "new_text=new\n"
                "END_OPERATION\n") >= 0 &&
            fclose(operation_test_file) == 0) {
            operation_test_file = NULL;
            operation_test_ok =
                !llm_exec_validate_ops_file(
                    operation_test_path);
        }
    }

    if (operation_test_file != NULL) {
        (void)fclose(operation_test_file);
    }

    if (operation_test_created) {
        (void)remove(operation_test_path);
    }

    llm_selftest_report(
        "operation_count mismatch rejected",
        operation_test_ok,
        &passed_count,
        &failed_count
    );
}

{
    unsigned int count;
    int ok;

    count = 999U;
    ok = llm_execute_count_ops(
        "inspection only\n"
        "operation_count=0\n",
        &count);

    llm_selftest_report(
        "count zero operations",
        ok && count == 0U,
        &passed_count,
        &failed_count
    );
}

{
    unsigned int count;
    int ok;

    count = 0U;
    ok = llm_execute_count_ops(
        "operation_count=1\n"
        "BEGIN_OPERATION\n"
        "END_OPERATION\n",
        &count);

    llm_selftest_report(
        "count one operation",
        ok && count == 1U,
        &passed_count,
        &failed_count
    );
}

{
    unsigned int count;
    int ok;

    count = 0U;
    ok = llm_execute_count_ops(
        "operation_count=19\n"
        "BEGIN_OPERATION\n"
        "END_OPERATION\n",
        &count);

    llm_selftest_report(
        "count ignores declared mismatch",
        ok && count == 1U,
        &passed_count,
        &failed_count
    );
}

{
    unsigned int count;
    int ok;

    count = 0U;
    ok = llm_execute_count_ops(
        "operation_count=1\r\n"
        "BEGIN_OPERATION\r\n"
        "END_OPERATION\r\n",
        &count);

    llm_selftest_report(
        "count supports CRLF",
        ok && count == 1U,
        &passed_count,
        &failed_count
    );
}

{
    unsigned int count;
    int ok;

    count = 0U;
    ok = llm_execute_count_ops(
        "BEGIN_OPERATION\n"
        "END_OPERATION",
        &count);

    llm_selftest_report(
        "count supports no final newline",
        ok && count == 1U,
        &passed_count,
        &failed_count
    );
}

{
    unsigned int count;
    int ok;

    count = 777U;
    ok = llm_execute_count_ops(
        "BEGIN_OPERATION\n"
        "BEGIN_OPERATION\n"
        "END_OPERATION\n"
        "END_OPERATION\n",
        &count);

    llm_selftest_report(
        "count rejects nested operations",
        !ok && count == 777U,
        &passed_count,
        &failed_count
    );
}

{
    unsigned int count;
    int begin_ok;
    int end_ok;

    count = 777U;
    begin_ok = llm_execute_count_ops(
        "BEGIN_OPERATION\n",
        &count);

    count = 777U;
    end_ok = llm_execute_count_ops(
        "END_OPERATION\n",
        &count);

    llm_selftest_report(
        "count rejects unmatched operations",
        !begin_ok && !end_ok,
        &passed_count,
        &failed_count
    );
}

{
    char too_many[2048];
    unsigned int count;
    unsigned int index;
    int ok;

    too_many[0] = '\0';

    for (index = 0U; index < 33U; ++index) {
        (void)strcat(
            too_many,
            "BEGIN_OPERATION\n"
            "END_OPERATION\n"
        );
    }

    count = 777U;
    ok = llm_execute_count_ops(
        too_many,
        &count);

    llm_selftest_report(
        "count rejects too many operations",
        !ok && count == 777U,
        &passed_count,
        &failed_count
    );
}
