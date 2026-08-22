{
    const char *path =
        "OVMS_AGENT_TEST_M139_CLEAR.TMP";
    int ok;

    ok = openai_m139_clear_sidecar(path);
    (void)openai_plan_clear_files(path);

    openai_selftest_report(
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

    (void)openai_plan_clear_files(path);
    ok = openai_m139_missing_checksum(path);
    (void)openai_plan_clear_files(path);

    openai_selftest_report(
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

    (void)openai_plan_clear_files(path);
    ok = openai_m139_orphan_checksum(path);
    (void)openai_plan_clear_files(path);

    openai_selftest_report(
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

    (void)openai_plan_clear_files(plan);
    (void)openai_plan_clear_files(fresh);
    (void)openai_plan_clear_files(backup);

    ok = openai_m139_consumed_checksum(
        plan,
        fresh,
        backup);

    (void)openai_plan_clear_files(plan);
    (void)openai_plan_clear_files(fresh);
    (void)openai_plan_clear_files(backup);

    openai_selftest_report(
        "consumed plan retains valid checksum",
        ok,
        &passed_count,
        &failed_count
    );
}
