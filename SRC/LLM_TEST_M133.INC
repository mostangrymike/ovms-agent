{
    int ok;

    ok = llm_m133_reset_current();

    llm_selftest_report(
        "reset helper clears current state",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = llm_m133_reset_snapshot();

    llm_selftest_report(
        "reset helper clears saved snapshot",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = llm_m133_reset_flags();

    llm_selftest_report(
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

    ok = llm_m133_load_repopulates(path);

    while (remove(path) == 0) {
    }

    llm_selftest_report(
        "successful load repopulates reset state",
        ok,
        &passed_count,
        &failed_count
    );
}
