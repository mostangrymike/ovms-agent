{
    const char *path = "OVMS_AGENT_TEST_M131_WORK.TMP";
    int ok;

    while (remove(path) == 0) {
    }

    ok = openai_m131_clear_workflow(path);

    while (remove(path) == 0) {
    }

    openai_selftest_report(
        "failed load clears saved workflow",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    const char *path = "OVMS_AGENT_TEST_M131_BUILD.TMP";
    int ok;

    while (remove(path) == 0) {
    }

    ok = openai_m131_clear_build(path);

    while (remove(path) == 0) {
    }

    openai_selftest_report(
        "failed load clears saved build state",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    const char *path = "OVMS_AGENT_TEST_M131_ROLL.TMP";
    int ok;

    while (remove(path) == 0) {
    }

    ok = openai_m131_clear_rollback(path);

    while (remove(path) == 0) {
    }

    openai_selftest_report(
        "failed load clears saved rollback",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = openai_m131_save_preserves();

    openai_selftest_report(
        "failed save still preserves prior snapshot",
        ok,
        &passed_count,
        &failed_count
    );
}
