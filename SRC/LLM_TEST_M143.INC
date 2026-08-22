{
    const char *path =
        "OVMS_AGENT_TEST_M143_INV.TMP";
    int ok;

    (void)llm_plan_clear_files(path);
    ok = llm_m143_first_inv_once(path);
    (void)llm_plan_clear_files(path);

    llm_selftest_report(
        "first invalidation logs once",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    const char *path =
        "OVMS_AGENT_TEST_M143_DUP.TMP";
    int ok;

    (void)llm_plan_clear_files(path);
    ok = llm_m143_repeat_no_dup(path);
    (void)llm_plan_clear_files(path);

    llm_selftest_report(
        "repeated invalidation does not duplicate log",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = llm_m143_cancel_once();

    llm_selftest_report(
        "cancelled approval logs once",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = llm_m143_fail_once();

    llm_selftest_report(
        "failed approval logs once",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    const char *path =
        "OVMS_AGENT_TEST_M143_SAME.TMP";
    int ok;

    (void)llm_plan_clear_files(path);
    ok = llm_m143_same_recovery(path);
    (void)llm_plan_clear_files(path);

    llm_selftest_report(
        "identical recovery preserves approval",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    const char *path =
        "OVMS_AGENT_TEST_M143_DIFF.TMP";
    int ok;

    (void)llm_plan_clear_files(path);
    ok = llm_m143_diff_recovery(path);
    (void)llm_plan_clear_files(path);

    llm_selftest_report(
        "different recovery invalidates approval",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    const char *path =
        "OVMS_AGENT_TEST_M143_CONSUMED.TMP";
    int ok;

    (void)llm_plan_clear_files(path);
    ok = llm_m143_consumed_clear(path);
    (void)llm_plan_clear_files(path);

    llm_selftest_report(
        "consumed recovery clears approval",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = llm_m143_partial_clear_safe();

    llm_selftest_report(
        "partial clear leaves safe approval state",
        ok,
        &passed_count,
        &failed_count
    );
}
