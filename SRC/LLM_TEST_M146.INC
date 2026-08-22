{
    int ok;

    ok = llm_m146_lower_y();

    llm_selftest_report(
        "lowercase y grants approval",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = llm_m146_upper_y();

    llm_selftest_report(
        "uppercase Y grants approval",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = llm_m146_yes_reject();

    llm_selftest_report(
        "yes does not grant approval",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = llm_m146_empty_reject();

    llm_selftest_report(
        "empty response does not grant approval",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = llm_m146_space_reject();

    llm_selftest_report(
        "whitespace response does not grant approval",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = llm_m146_eof_reject();

    llm_selftest_report(
        "EOF does not grant approval",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = llm_m146_long_reject();

    llm_selftest_report(
        "overlong response does not grant approval",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = llm_m146_alt_reject();

    llm_selftest_report(
        "alternate command cannot grant approval",
        ok,
        &passed_count,
        &failed_count
    );
}
