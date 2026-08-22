{
    FILE *target_file;
    FILE *plan_file;
    int operation_test_ok;
    int target_created;
    int plan_created;
    const char *target_path =
        "OVMS_AGENT_TEST_STALE_TARGET.TMP";
    const char *plan_path =
        "OVMS_AGENT_TEST_STALE_PLAN.TMP";

    operation_test_ok = 0;
    target_created = 0;
    plan_created = 0;
    target_file = NULL;
    plan_file = NULL;

    if (!llm_selftest_file_exists(target_path) &&
        !llm_selftest_file_exists(plan_path)) {
        target_file = fopen(target_path, "w");

        if (target_file != NULL) {
            target_created = 1;
        }
    }

    if (target_file != NULL) {
        if (fputs("A", target_file) != EOF &&
            fclose(target_file) == 0) {
            target_file = NULL;
            plan_file = fopen(plan_path, "w");

            if (plan_file != NULL) {
                plan_created = 1;
            }
        }
    }

    if (plan_file != NULL) {
        if (fprintf(
                plan_file,
                "format=1\n"
                "status=active\n"
                "file_count=1\n"
                "file=%s|size=999|modified=0\n",
                target_path) >= 0 &&
            fclose(plan_file) == 0) {
            plan_file = NULL;
            operation_test_ok =
                !llm_plan_file_current(
                    plan_path,
                    0);
        }
    }

    if (target_file != NULL) {
        (void)fclose(target_file);
    }

    if (plan_file != NULL) {
        (void)fclose(plan_file);
    }

    if (plan_created) {
        (void)remove(plan_path);
    }

    if (target_created) {
        (void)remove(target_path);
    }

    llm_selftest_report(
        "dry run rejects stale plan",
        operation_test_ok,
        &passed_count,
        &failed_count
    );
}
