{
    int ok;

    ok = openai_m144_approved_fixture();

    openai_selftest_report(
        "approved fixture uses production approval",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    const char *path =
        "OVMS_AGENT_TEST_M144_INVALID.TMP";
    int ok;

    (void)openai_plan_clear_files(path);
    ok = openai_m144_invalid_fixture(path);
    (void)openai_plan_clear_files(path);
    openai_plan_approval_clear();

    openai_selftest_report(
        "invalidated fixture uses production validation",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = openai_m144_unapproved_fixture();

    openai_selftest_report(
        "unapproved fixture uses production clear",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    const char *path =
        "OVMS_AGENT_TEST_M144_REAPPROVE.TMP";
    int ok;

    (void)openai_plan_clear_files(path);
    ok = openai_m144_reapprove_fixture(path);
    (void)openai_plan_clear_files(path);
    openai_plan_approval_clear();

    openai_selftest_report(
        "reapproval test uses no direct globals",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = openai_m144_prewrite_prod();

    openai_selftest_report(
        "pre-write consumption uses production transition",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = openai_m144_status_prod();

    openai_selftest_report(
        "status tests use production-created states",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    const char *path =
        "OVMS_AGENT_TEST_M144_RECOVERY.TMP";
    int ok;

    (void)openai_plan_clear_files(path);
    ok = openai_m144_recovery_prod(path);
    (void)openai_plan_clear_files(path);
    openai_plan_approval_clear();

    openai_selftest_report(
        "recovery tests use production-created states",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = openai_m144_no_direct_globals();

    openai_selftest_report(
        "no M141-M144 test assigns approval globals",
        ok,
        &passed_count,
        &failed_count
    );
}
