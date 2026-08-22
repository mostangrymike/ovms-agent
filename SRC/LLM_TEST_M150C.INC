{
    int ok;

    ok = llm_m150c_has_create();

    llm_selftest_report(
        "M150C planner describes create_file",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = llm_m150c_guards_target();

    llm_selftest_report(
        "M150C planner guards create targets",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = llm_m150c_keeps_replace();

    llm_selftest_report(
        "M150C planner preserves replace_block",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = llm_m150c_blocks_legacy();

    llm_selftest_report(
        "M150C planner rejects legacy syntax",
        ok,
        &passed_count,
        &failed_count
    );
}
