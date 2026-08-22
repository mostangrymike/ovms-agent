{
    const char *path = "OVMS_AGENT_TEST_M110_DUP_WORKFLOW.TMP";
    int ok;

    while (remove(path) == 0) {
    }

    ok = llm_m110_dup_workflow(path);

    while (remove(path) == 0) {
    }

    llm_selftest_report(
        "duplicate workflow state rejected",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    const char *path = "OVMS_AGENT_TEST_M110_DUP_ROLLBACK.TMP";
    int ok;

    while (remove(path) == 0) {
    }

    ok = llm_m110_dup_rollback(path);

    while (remove(path) == 0) {
    }

    llm_selftest_report(
        "duplicate rollback state rejected",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    const char *path = "OVMS_AGENT_TEST_M110_UNKNOWN.TMP";
    int ok;

    while (remove(path) == 0) {
    }

    ok = llm_m110_unknown_field(path);

    while (remove(path) == 0) {
    }

    llm_selftest_report(
        "unknown state field rejected",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    const char *path = "OVMS_AGENT_TEST_M110_SUFFIX.TMP";
    int ok;

    while (remove(path) == 0) {
    }

    ok = llm_m110_value_suffix(path);

    while (remove(path) == 0) {
    }

    llm_selftest_report(
        "state value suffix rejected",
        ok,
        &passed_count,
        &failed_count
    );
}
