{
    int ok;

    ok = llm_m149_single_line();

    llm_selftest_report(
        "M149 single-line compatibility",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = llm_m149_two_line();

    llm_selftest_report(
        "M149 two-line continuation",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = llm_m149_three_line();

    llm_selftest_report(
        "M149 three-line continuation",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = llm_m149_marker_removed();

    llm_selftest_report(
        "M149 continuation marker removed",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = llm_m149_separator();

    llm_selftest_report(
        "M149 safe separator inserted",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = llm_m149_boundary_space();

    llm_selftest_report(
        "M149 existing whitespace preserved",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = llm_m149_odd_backslash();

    llm_selftest_report(
        "M149 odd trailing backslash continues",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = llm_m149_even_backslash();

    llm_selftest_report(
        "M149 even trailing backslashes literal",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = llm_m149_pending_eof();

    llm_selftest_report(
        "M149 pending continuation EOF rejected",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = llm_m149_complete_eof();

    llm_selftest_report(
        "M149 complete EOF record accepted",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = llm_m149_overflow();

    llm_selftest_report(
        "M149 assembled overflow rejected",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = llm_m149_drain_next();

    llm_selftest_report(
        "M149 rejected command drains safely",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = llm_m149_single_dispatch();

    llm_selftest_report(
        "M149 one logical command per read",
        ok,
        &passed_count,
        &failed_count
    );
}
