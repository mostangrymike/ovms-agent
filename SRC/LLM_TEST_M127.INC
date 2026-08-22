{
    int ok;

    ok = openai_m127_hide_workflow();

    openai_selftest_report(
        "invalid state hides workflow details",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = openai_m127_hide_build();

    openai_selftest_report(
        "invalid state hides build details",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = openai_m127_hide_rollback();

    openai_selftest_report(
        "invalid state hides rollback details",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = openai_m127_valid_details();

    openai_selftest_report(
        "valid state details remain visible",
        ok,
        &passed_count,
        &failed_count
    );
}
