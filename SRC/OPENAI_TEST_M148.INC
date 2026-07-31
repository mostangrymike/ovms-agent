{
    int ok;

    ok = openai_m148_complete_accept();

    openai_selftest_report(
        "complete command line accepted",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = openai_m148_exact_safe();

    openai_selftest_report(
        "exact-buffer line handled safely",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = openai_m148_prefix_blocked();

    openai_selftest_report(
        "overlong command prefix not dispatched",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = openai_m148_remainder_drained();

    openai_selftest_report(
        "overlong command remainder drained",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = openai_m148_approval_unchanged();

    openai_selftest_report(
        "overlong approval line leaves state unchanged",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = openai_m148_execute_no_action();

    openai_selftest_report(
        "overlong execute line performs no action",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = openai_m148_eof_accept();

    openai_selftest_report(
        "EOF-terminated final command accepted",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = openai_m148_empty_eof();

    openai_selftest_report(
        "empty EOF terminates without dispatch",
        ok,
        &passed_count,
        &failed_count
    );
}
