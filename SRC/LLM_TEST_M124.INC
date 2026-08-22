{
    const char *path = "OVMS_AGENT_TEST_M124_NORMAL.TMP";
    int ok;

    while (remove(path) == 0) {
    }

    ok = llm_m124_normal_load(path);

    while (remove(path) == 0) {
    }

    llm_selftest_report(
        "normal state load not marked recovered",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    const char *path = "OVMS_AGENT_TEST_M124_FALLBACK.TMP";
    int ok;

    while (remove(path) == 0) {
    }

    ok = llm_m124_fallback_marked(path);

    while (remove(path) == 0) {
    }

    llm_selftest_report(
        "fallback state load marked recovered",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = llm_m124_save_clears();

    llm_selftest_report(
        "successful save clears recovery marker",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = llm_m124_clear_clears();

    llm_selftest_report(
        "state clear clears recovery marker",
        ok,
        &passed_count,
        &failed_count
    );
}
