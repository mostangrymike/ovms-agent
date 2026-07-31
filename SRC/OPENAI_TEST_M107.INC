{
    const char *path = "OVMS_AGENT_TEST_FAILED_BUILD.TMP";
    int ok;

    while (remove(path) == 0) {
    }

    ok = openai_m107_build_evidence(path);

    while (remove(path) == 0) {
    }

    openai_selftest_report(
        "failed-build evidence file written",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    const char *path = "OVMS_AGENT_TEST_FAILED_OPERATIONS.TMP";
    int ok;

    while (remove(path) == 0) {
    }

    ok = openai_m107_ops_evidence(path);

    while (remove(path) == 0) {
    }

    openai_selftest_report(
        "failed-operation evidence file written",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    const char *path = "OVMS_AGENT_TEST_STATE_SERIALIZE.TMP";
    int ok;

    while (remove(path) == 0) {
    }

    ok = openai_m107_state_serialize(path);

    while (remove(path) == 0) {
    }

    openai_selftest_report(
        "rollback state serializes success",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    const char *path = "OVMS_AGENT_TEST_STATE_RELOAD.TMP";
    int ok;

    while (remove(path) == 0) {
    }

    ok = openai_m107_state_reload(path);

    while (remove(path) == 0) {
    }

    openai_selftest_report(
        "execute workflow state reload accepted",
        ok,
        &passed_count,
        &failed_count
    );
}
