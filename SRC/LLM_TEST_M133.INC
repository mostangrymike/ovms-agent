{
    int ok;

    ok = openai_m133_reset_current();

    openai_selftest_report(
        "reset helper clears current state",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = openai_m133_reset_snapshot();

    openai_selftest_report(
        "reset helper clears saved snapshot",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = openai_m133_reset_flags();

    openai_selftest_report(
        "reset helper clears state-status flags",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    const char *path = "OVMS_AGENT_TEST_M133_LOAD.TMP";
    int ok;

    while (remove(path) == 0) {
    }

    ok = openai_m133_load_repopulates(path);

    while (remove(path) == 0) {
    }

    openai_selftest_report(
        "successful load repopulates reset state",
        ok,
        &passed_count,
        &failed_count
    );
}
