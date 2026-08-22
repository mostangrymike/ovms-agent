{
    int ok;

    ok = openai_m149_single_line();

    openai_selftest_report(
        "M149 single-line compatibility",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = openai_m149_two_line();

    openai_selftest_report(
        "M149 two-line continuation",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = openai_m149_three_line();

    openai_selftest_report(
        "M149 three-line continuation",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = openai_m149_marker_removed();

    openai_selftest_report(
        "M149 continuation marker removed",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = openai_m149_separator();

    openai_selftest_report(
        "M149 safe separator inserted",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = openai_m149_boundary_space();

    openai_selftest_report(
        "M149 existing whitespace preserved",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = openai_m149_odd_backslash();

    openai_selftest_report(
        "M149 odd trailing backslash continues",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = openai_m149_even_backslash();

    openai_selftest_report(
        "M149 even trailing backslashes literal",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = openai_m149_pending_eof();

    openai_selftest_report(
        "M149 pending continuation EOF rejected",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = openai_m149_complete_eof();

    openai_selftest_report(
        "M149 complete EOF record accepted",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = openai_m149_overflow();

    openai_selftest_report(
        "M149 assembled overflow rejected",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = openai_m149_drain_next();

    openai_selftest_report(
        "M149 rejected command drains safely",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = openai_m149_single_dispatch();

    openai_selftest_report(
        "M149 one logical command per read",
        ok,
        &passed_count,
        &failed_count
    );
}
