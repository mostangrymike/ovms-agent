{
    int ok;

    ok = llm_m147_exact_dispatch();

    llm_selftest_report(
        "exact approval command dispatches",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = llm_m147_lower_dispatch();

    llm_selftest_report(
        "lowercase approval command dispatches",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = llm_m147_abbrev_reject();

    llm_selftest_report(
        "abbreviated approval command rejected",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = llm_m147_suffix_reject();

    llm_selftest_report(
        "approval suffix command rejected",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = llm_m147_argument_reject();

    llm_selftest_report(
        "approval argument rejected before prompt",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = llm_m147_duplicate_reject();

    llm_selftest_report(
        "duplicate approval token rejected",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = llm_m147_space_accept();

    llm_selftest_report(
        "whitespace-only arguments accepted",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = llm_m147_reject_unchanged();

    llm_selftest_report(
        "rejected command leaves approval unchanged",
        ok,
        &passed_count,
        &failed_count
    );
}
