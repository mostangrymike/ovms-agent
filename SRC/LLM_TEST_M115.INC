{
    const char *path =
        "OVMS_AGENT_TEST_M115_BAD_WORKFLOW.TMP";
    int ok;

    while (remove(path) == 0) {
    }

    ok = llm_m115_bad_workflow(path);

    while (remove(path) == 0) {
    }

    llm_selftest_report(
        "writer rejects invalid workflow",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    const char *path =
        "OVMS_AGENT_TEST_M115_BAD_BUILD_FLAG.TMP";
    int ok;

    while (remove(path) == 0) {
    }

    ok = llm_m115_bad_build_flag(path);

    while (remove(path) == 0) {
    }

    llm_selftest_report(
        "writer rejects invalid build flag",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    const char *path =
        "OVMS_AGENT_TEST_M115_BAD_BUILD_STATE.TMP";
    int ok;

    while (remove(path) == 0) {
    }

    ok = llm_m115_bad_build_state(path);

    while (remove(path) == 0) {
    }

    llm_selftest_report(
        "writer rejects inconsistent build",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    const char *path =
        "OVMS_AGENT_TEST_M115_BAD_ROLLBACK.TMP";
    int ok;

    while (remove(path) == 0) {
    }

    ok = llm_m115_bad_rollback(path);

    while (remove(path) == 0) {
    }

    llm_selftest_report(
        "writer rejects invalid rollback",
        ok,
        &passed_count,
        &failed_count
    );
}
