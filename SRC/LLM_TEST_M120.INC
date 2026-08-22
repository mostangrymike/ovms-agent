{
    const char *path = "OVMS_AGENT_TEST_M120_RECOVERY.TMP";
    int ok;

    while (remove(path) == 0) {
    }

    ok = llm_m120_recovery_version(path);

    while (remove(path) == 0) {
    }

    llm_selftest_report(
        "state save retains recovery version",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    const char *path = "OVMS_AGENT_TEST_M120_BOUNDED.TMP";
    int ok;

    while (remove(path) == 0) {
    }

    ok = llm_m120_versions_bounded(path);

    while (remove(path) == 0) {
    }

    llm_selftest_report(
        "state versions are bounded",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    const char *path = "OVMS_AGENT_TEST_M120_NEWEST.TMP";
    int ok;

    while (remove(path) == 0) {
    }

    ok = llm_m120_newest_preserved(path);

    while (remove(path) == 0) {
    }

    llm_selftest_report(
        "pruning never removes newest state",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = llm_m120_unsafe_rejected();

    llm_selftest_report(
        "unsafe prune path rejected",
        ok,
        &passed_count,
        &failed_count
    );
}
