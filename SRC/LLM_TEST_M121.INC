{
    int ok;

    ok = openai_m121_success_flags();

    openai_selftest_report(
        "successful save marks state valid",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = openai_m121_failure_valid();

    openai_selftest_report(
        "failed save clears state-valid flag",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = openai_m121_failure_loaded();

    openai_selftest_report(
        "failed save retains loaded-state flag",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    const char *path =
        "OVMS_AGENT_TEST_M121_PRESERVE.TMP";
    int ok;

    while (remove(path) == 0) {
    }

    ok = openai_m121_disk_preserved(path);

    while (remove(path) == 0) {
    }

    openai_selftest_report(
        "failed save preserves prior disk state",
        ok,
        &passed_count,
        &failed_count
    );
}
