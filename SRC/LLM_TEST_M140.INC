{
    const char *path =
        "OVMS_AGENT_TEST_M140_UNAPPROVED.TMP";
    int ok;

    (void)openai_plan_clear_files(path);
    ok = openai_m140_unapproved_reject(path);
    (void)openai_plan_clear_files(path);

    openai_selftest_report(
        "unapproved current plan cannot execute",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    const char *path =
        "OVMS_AGENT_TEST_M140_APPROVED.TMP";
    int ok;

    (void)openai_plan_clear_files(path);
    ok = openai_m140_approved_accept(path);
    (void)openai_plan_clear_files(path);

    openai_selftest_report(
        "explicit approval permits unchanged plan",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    const char *path =
        "OVMS_AGENT_TEST_M140_CHANGED.TMP";
    int ok;

    (void)openai_plan_clear_files(path);
    ok = openai_m140_change_reject(path);
    (void)openai_plan_clear_files(path);

    openai_selftest_report(
        "plan change invalidates session approval",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    const char *path =
        "OVMS_AGENT_TEST_M140_NEW.TMP";
    int ok;

    (void)openai_plan_clear_files(path);
    ok = openai_m140_new_plan_clears(path);
    (void)openai_plan_clear_files(path);

    openai_selftest_report(
        "new plan clears prior approval",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    const char *path =
        "OVMS_AGENT_TEST_M140_CLEAR.TMP";
    int ok;

    (void)openai_plan_clear_files(path);
    ok = openai_m140_clear_clears(path);
    (void)openai_plan_clear_files(path);

    openai_selftest_report(
        "plan clear clears session approval",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = openai_m140_consume_clears();

    openai_selftest_report(
        "consumption clears session approval",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = openai_m140_restart_unapproved();

    openai_selftest_report(
        "restart begins without approval",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    const char *path =
        "OVMS_AGENT_TEST_M140_PREWRITE.TMP";
    int ok;

    (void)openai_plan_clear_files(path);
    ok = openai_m140_prewrite_guard(path);
    (void)openai_plan_clear_files(path);

    openai_selftest_report(
        "approval check precedes first project write",
        ok,
        &passed_count,
        &failed_count
    );
}
