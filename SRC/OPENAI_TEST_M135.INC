{
    const char *ops =
        "OVMS_AGENT_TEST_M135_SHARED_OPS.TMP";
    const char *target =
        "OVMS_AGENT_TEST_M135_SHARED.TMP";
    int ok;

    while (remove(ops) == 0) {
    }
    while (remove(target) == 0) {
    }

    ok = openai_m135_shared_validation(
        ops,
        target);

    while (remove(ops) == 0) {
    }
    while (remove(target) == 0) {
    }

    openai_selftest_report(
        "dry run and execution share validation",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    const char *ops =
        "OVMS_AGENT_TEST_M135_PREWRITE_OPS.TMP";
    const char *target =
        "OVMS_AGENT_TEST_M135_PREWRITE.TMP";
    int ok;

    while (remove(ops) == 0) {
    }
    while (remove(target) == 0) {
    }

    ok = openai_m135_prewrite_validation(
        ops,
        target);

    while (remove(ops) == 0) {
    }
    while (remove(target) == 0) {
    }

    openai_selftest_report(
        "all operations validate before modification",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    const char *ops =
        "OVMS_AGENT_TEST_M135_CHAIN_OPS.TMP";
    const char *target =
        "OVMS_AGENT_TEST_M135_CHAIN.TMP";
    int ok;

    while (remove(ops) == 0) {
    }
    while (remove(target) == 0) {
    }

    ok = openai_m135_chain_deterministic(
        ops,
        target);

    while (remove(ops) == 0) {
    }
    while (remove(target) == 0) {
    }

    openai_selftest_report(
        "same-file staging is deterministic",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    const char *ops =
        "OVMS_AGENT_TEST_M135_ROLL_OPS.TMP";
    const char *target =
        "OVMS_AGENT_TEST_M135_ROLLBACK.TMP";
    int ok;

    while (remove(ops) == 0) {
    }
    while (remove(target) == 0) {
    }

    ok = openai_m135_rollback_all(
        ops,
        target);

    while (remove(ops) == 0) {
    }
    while (remove(target) == 0) {
    }

    openai_selftest_report(
        "rollback coverage includes every changed file",
        ok,
        &passed_count,
        &failed_count
    );
}
