{
    int ok;

    ok = openai_m126_invalid_source();

    openai_selftest_report(
        "invalid state source reports unavailable",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = openai_m126_newest_source();

    openai_selftest_report(
        "newest state source remains accurate",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = openai_m126_recovery_source();

    openai_selftest_report(
        "recovered state source remains accurate",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = openai_m126_invalid_overrides();

    openai_selftest_report(
        "state display suppresses invalid source",
        ok,
        &passed_count,
        &failed_count
    );
}
