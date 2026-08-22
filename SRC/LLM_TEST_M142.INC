{
    int ok;

    ok = openai_m142_cancel_unapproved();

    openai_selftest_report(
        "cancelled approval remains unapproved",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = openai_m142_fail_unapproved();

    openai_selftest_report(
        "failed approval remains unapproved",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = openai_m142_validate_keeps();

    openai_selftest_report(
        "unchanged validation preserves approval",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = openai_m142_prewrite_keeps();

    openai_selftest_report(
        "pre-write failure preserves approval",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = openai_m142_postwrite_clears();

    openai_selftest_report(
        "post-write failure clears approval",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = openai_m142_success_clears();

    openai_selftest_report(
        "successful execution clears approval",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    const char *path =
        "OVMS_AGENT_TEST_M142_CHECKSUM.TMP";
    int ok;

    (void)openai_plan_clear_files(path);
    ok = openai_m142_checksum_invalid(path);
    (void)openai_plan_clear_files(path);

    openai_selftest_report(
        "checksum loss reports invalidated approval",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    const char *path =
        "OVMS_AGENT_TEST_M142_REAPPROVE.TMP";
    int ok;

    (void)openai_plan_clear_files(path);
    ok = openai_m142_reapprove_clears(path);
    (void)openai_plan_clear_files(path);

    openai_selftest_report(
        "reapproval clears invalidated state",
        ok,
        &passed_count,
        &failed_count
    );
}
