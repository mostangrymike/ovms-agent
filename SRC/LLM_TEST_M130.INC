{
    int ok;

    ok = llm_m130_first_fail_invalid();

    llm_selftest_report(
        "failed first save remains invalid",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = llm_m130_later_fail_valid();

    llm_selftest_report(
        "failed later save preserves valid snapshot",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = llm_m130_success_valid();

    llm_selftest_report(
        "successful save establishes validity",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    const char *path = "OVMS_AGENT_TEST_M130_LOAD.TMP";
    int ok;

    while (remove(path) == 0) {
    }

    ok = llm_m130_load_resets_save(path);

    while (remove(path) == 0) {
    }

    llm_selftest_report(
        "load resets save-attempt status",
        ok,
        &passed_count,
        &failed_count
    );
}
