{
    int ok;

    ok = llm_m144_approved_fixture();

    llm_selftest_report(
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

    (void)llm_plan_clear_files(path);
    ok = llm_m144_invalid_fixture(path);
    (void)llm_plan_clear_files(path);
    llm_plan_approval_clear();

    llm_selftest_report(
        "invalidated fixture uses production validation",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = llm_m144_unapproved_fixture();

    llm_selftest_report(
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

    (void)llm_plan_clear_files(path);
    ok = llm_m144_reapprove_fixture(path);
    (void)llm_plan_clear_files(path);
    llm_plan_approval_clear();

    llm_selftest_report(
        "reapproval test uses no direct globals",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = llm_m144_prewrite_prod();

    llm_selftest_report(
        "pre-write consumption uses production transition",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = llm_m144_status_prod();

    llm_selftest_report(
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

    (void)llm_plan_clear_files(path);
    ok = llm_m144_recovery_prod(path);
    (void)llm_plan_clear_files(path);
    llm_plan_approval_clear();

    llm_selftest_report(
        "recovery tests use production-created states",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = llm_m144_no_direct_globals();

    llm_selftest_report(
        "no M141-M144 test assigns approval globals",
        ok,
        &passed_count,
        &failed_count
    );
}
