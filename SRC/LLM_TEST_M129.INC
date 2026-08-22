{
    int ok;

    ok = openai_m129_fail_keeps_valid();

    openai_selftest_report(
        "failed save preserves snapshot validity",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = openai_m129_fail_keeps_values();

    openai_selftest_report(
        "failed save preserves snapshot values",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = openai_m129_failure_reported();

    openai_selftest_report(
        "failed save status is reported",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = openai_m129_success_clears();

    openai_selftest_report(
        "successful save clears failure status",
        ok,
        &passed_count,
        &failed_count
    );
}
