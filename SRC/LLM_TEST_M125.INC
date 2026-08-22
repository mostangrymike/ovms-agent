{
    const char *path = "OVMS_AGENT_TEST_M125_FAIL1.TMP";
    int ok;

    while (remove(path) == 0) {
    }

    ok = openai_m125_fail_clears(path);

    while (remove(path) == 0) {
    }

    openai_selftest_report(
        "failed state load clears recovery marker",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    const char *path = "OVMS_AGENT_TEST_M125_FAIL2.TMP";
    int ok;

    while (remove(path) == 0) {
    }

    ok = openai_m125_fail_invalid(path);

    while (remove(path) == 0) {
    }

    openai_selftest_report(
        "failed state load remains invalid",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    const char *path = "OVMS_AGENT_TEST_M125_NORMAL.TMP";
    int ok;

    while (remove(path) == 0) {
    }

    ok = openai_m125_normal_resets(path);

    while (remove(path) == 0) {
    }

    openai_selftest_report(
        "normal reload clears prior recovery marker",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    const char *path = "OVMS_AGENT_TEST_M125_RECOVER.TMP";
    int ok;

    while (remove(path) == 0) {
    }

    ok = openai_m125_recovery_sets(path);

    while (remove(path) == 0) {
    }

    openai_selftest_report(
        "recovery reload sets marker again",
        ok,
        &passed_count,
        &failed_count
    );
}
