{
    const char *path =
        "OVMS_AGENT_TEST_M118_VERSIONS.TMP";
    int ok;

    while (remove(path) == 0) {
    }

    ok = llm_m118_purge_versions(path);

    while (remove(path) == 0) {
    }

    llm_selftest_report(
        "state purge removes all versions",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    const char *path =
        "OVMS_AGENT_TEST_M118_MISSING.TMP";
    int ok;

    while (remove(path) == 0) {
    }

    ok = llm_m118_purge_missing(path);

    llm_selftest_report(
        "state purge accepts missing file",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = llm_m118_purge_empty();

    llm_selftest_report(
        "state purge rejects empty path",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    const char *path =
        "OVMS_AGENT_TEST_M118_CLEAR.TMP";
    int ok;

    while (remove(path) == 0) {
    }

    ok = llm_m118_state_stays_clear(path);

    while (remove(path) == 0) {
    }

    llm_selftest_report(
        "cleared state does not reappear",
        ok,
        &passed_count,
        &failed_count
    );
}
