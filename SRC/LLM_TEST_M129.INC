{
    int ok;

    ok = llm_m129_fail_keeps_valid();

    llm_selftest_report(
        "failed save preserves snapshot validity",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = llm_m129_fail_keeps_values();

    llm_selftest_report(
        "failed save preserves snapshot values",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = llm_m129_failure_reported();

    llm_selftest_report(
        "failed save status is reported",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = llm_m129_success_clears();

    llm_selftest_report(
        "successful save clears failure status",
        ok,
        &passed_count,
        &failed_count
    );
}
