{
    const char *path =
        "OVMS_AGENT_TEST_M139_CLEAR.TMP";
    int ok;

    ok = llm_m139_clear_sidecar(path);
    (void)llm_plan_clear_files(path);

    llm_selftest_report(
        "plan clear removes checksum sidecar",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    const char *path =
        "OVMS_AGENT_TEST_M139_MISSING.TMP";
    int ok;

    (void)llm_plan_clear_files(path);
    ok = llm_m139_missing_checksum(path);
    (void)llm_plan_clear_files(path);

    llm_selftest_report(
        "missing checksum invalidates existing plan",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    const char *path =
        "OVMS_AGENT_TEST_M139_ORPHAN.TMP";
    int ok;

    (void)llm_plan_clear_files(path);
    ok = llm_m139_orphan_checksum(path);
    (void)llm_plan_clear_files(path);

    llm_selftest_report(
        "orphan checksum cannot create executable plan",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    const char *plan =
        "OVMS_AGENT_TEST_M139_CONSUMED.TMP";
    const char *fresh =
        "OVMS_AGENT_TEST_M139_NEW.TMP";
    const char *backup =
        "OVMS_AGENT_TEST_M139_OLD.TMP";
    int ok;

    (void)llm_plan_clear_files(plan);
    (void)llm_plan_clear_files(fresh);
    (void)llm_plan_clear_files(backup);

    ok = llm_m139_consumed_checksum(
        plan,
        fresh,
        backup);

    (void)llm_plan_clear_files(plan);
    (void)llm_plan_clear_files(fresh);
    (void)llm_plan_clear_files(backup);

    llm_selftest_report(
        "consumed plan retains valid checksum",
        ok,
        &passed_count,
        &failed_count
    );
}
