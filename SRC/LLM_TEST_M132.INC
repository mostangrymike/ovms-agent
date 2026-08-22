{
    const char *path = "OVMS_AGENT_TEST_M132_WORK.TMP";
    int ok;

    while (remove(path) == 0) {
    }

    ok = llm_m132_clear_current_work(path);

    while (remove(path) == 0) {
    }

    llm_selftest_report(
        "failed load clears current workflow",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    const char *path = "OVMS_AGENT_TEST_M132_BUILD.TMP";
    int ok;

    while (remove(path) == 0) {
    }

    ok = llm_m132_clear_current_build(path);

    while (remove(path) == 0) {
    }

    llm_selftest_report(
        "failed load clears current build state",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    const char *path = "OVMS_AGENT_TEST_M132_ROLL.TMP";
    int ok;

    while (remove(path) == 0) {
    }

    ok = llm_m132_clear_current_roll(path);

    while (remove(path) == 0) {
    }

    llm_selftest_report(
        "failed load clears current rollback",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = llm_m132_fail_keeps_current();

    llm_selftest_report(
        "failed save leaves current values unchanged",
        ok,
        &passed_count,
        &failed_count
    );
}
