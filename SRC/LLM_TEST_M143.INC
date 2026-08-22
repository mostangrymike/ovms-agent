{
    const char *path =
        "OVMS_AGENT_TEST_M143_INV.TMP";
    int ok;

    (void)openai_plan_clear_files(path);
    ok = openai_m143_first_inv_once(path);
    (void)openai_plan_clear_files(path);

    openai_selftest_report(
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

    (void)openai_plan_clear_files(path);
    ok = openai_m143_repeat_no_dup(path);
    (void)openai_plan_clear_files(path);

    openai_selftest_report(
        "repeated invalidation does not duplicate log",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = openai_m143_cancel_once();

    openai_selftest_report(
        "cancelled approval logs once",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = openai_m143_fail_once();

    openai_selftest_report(
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

    (void)openai_plan_clear_files(path);
    ok = openai_m143_same_recovery(path);
    (void)openai_plan_clear_files(path);

    openai_selftest_report(
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

    (void)openai_plan_clear_files(path);
    ok = openai_m143_diff_recovery(path);
    (void)openai_plan_clear_files(path);

    openai_selftest_report(
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

    (void)openai_plan_clear_files(path);
    ok = openai_m143_consumed_clear(path);
    (void)openai_plan_clear_files(path);

    openai_selftest_report(
        "consumed recovery clears approval",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = openai_m143_partial_clear_safe();

    openai_selftest_report(
        "partial clear leaves safe approval state",
        ok,
        &passed_count,
        &failed_count
    );
}
