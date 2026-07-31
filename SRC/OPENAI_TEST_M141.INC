{
    int ok;

    ok = openai_m141_status_unapproved();

    openai_selftest_report(
        "status reports unapproved plan",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = openai_m141_status_approved();

    openai_selftest_report(
        "status reports approved plan",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = openai_m141_show_no_grant();

    openai_selftest_report(
        "plan show does not grant approval",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = openai_m141_validate_no_grant();

    openai_selftest_report(
        "plan validate does not grant approval",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = openai_m141_dry_unapproved();

    openai_selftest_report(
        "dry run preserves unapproved state",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = openai_m141_dry_approved();

    openai_selftest_report(
        "dry run preserves valid approval",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    const char *path =
        "OVMS_AGENT_TEST_M141_CHANGED.TMP";
    int ok;

    (void)openai_plan_clear_files(path);
    ok = openai_m141_change_invalidated(path);
    (void)openai_plan_clear_files(path);

    openai_selftest_report(
        "plan change reports invalidated approval",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = openai_m141_refusal_message();

    openai_selftest_report(
        "execute refusal explains missing approval",
        ok,
        &passed_count,
        &failed_count
    );
}
