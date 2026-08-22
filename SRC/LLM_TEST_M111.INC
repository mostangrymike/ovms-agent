{
    const char *path =
        "OVMS_AGENT_TEST_M111_WORKFLOW_FIRST.TMP";
    int ok;

    while (remove(path) == 0) {
    }

    ok = llm_m111_workflow_first(path);

    while (remove(path) == 0) {
    }

    llm_selftest_report(
        "workflow before format rejected",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    const char *path =
        "OVMS_AGENT_TEST_M111_STATUS_FIRST.TMP";
    int ok;

    while (remove(path) == 0) {
    }

    ok = llm_m111_status_before_known(path);

    while (remove(path) == 0) {
    }

    llm_selftest_report(
        "build status before build known rejected",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    const char *path =
        "OVMS_AGENT_TEST_M111_ROLLBACK_FIRST.TMP";
    int ok;

    while (remove(path) == 0) {
    }

    ok = llm_m111_rollback_first(path);

    while (remove(path) == 0) {
    }

    llm_selftest_report(
        "rollback before build status rejected",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    const char *path =
        "OVMS_AGENT_TEST_M111_CANONICAL.TMP";
    int ok;

    while (remove(path) == 0) {
    }

    ok = llm_m111_canonical_order(path);

    while (remove(path) == 0) {
    }

    llm_selftest_report(
        "canonical state field order accepted",
        ok,
        &passed_count,
        &failed_count
    );
}
