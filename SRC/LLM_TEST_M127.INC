{
    int ok;

    ok = llm_m127_hide_workflow();

    llm_selftest_report(
        "invalid state hides workflow details",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = llm_m127_hide_build();

    llm_selftest_report(
        "invalid state hides build details",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = llm_m127_hide_rollback();

    llm_selftest_report(
        "invalid state hides rollback details",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = llm_m127_valid_details();

    llm_selftest_report(
        "valid state details remain visible",
        ok,
        &passed_count,
        &failed_count
    );
}
