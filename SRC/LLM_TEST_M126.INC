{
    int ok;

    ok = llm_m126_invalid_source();

    llm_selftest_report(
        "invalid state source reports unavailable",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = llm_m126_newest_source();

    llm_selftest_report(
        "newest state source remains accurate",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = llm_m126_recovery_source();

    llm_selftest_report(
        "recovered state source remains accurate",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = llm_m126_invalid_overrides();

    llm_selftest_report(
        "state display suppresses invalid source",
        ok,
        &passed_count,
        &failed_count
    );
}
