{
    const char *path = "OVMS_AGENT_TEST_M128_LOAD.TMP";
    int ok;

    while (remove(path) == 0) {
    }

    ok = llm_m128_load_snapshot(path);

    while (remove(path) == 0) {
    }

    llm_selftest_report(
        "loaded state populates persisted snapshot",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = llm_m128_current_isolated();

    llm_selftest_report(
        "current workflow does not alter snapshot",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    const char *path = "OVMS_AGENT_TEST_M128_SAVE.TMP";
    int ok;

    while (remove(path) == 0) {
    }

    ok = llm_m128_save_refreshes(path);

    while (remove(path) == 0) {
    }

    llm_selftest_report(
        "successful save refreshes persisted snapshot",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = llm_m128_fail_hides_current();

    llm_selftest_report(
        "failed save does not expose current values",
        ok,
        &passed_count,
        &failed_count
    );
}
