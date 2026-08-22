{
    const char *target =
        "OVMS_AGENT_TEST_M136_EVIDENCE_TARGET.TMP";
    const char *evidence =
        "OVMS_AGENT_TEST_M136_EVIDENCE.TMP";
    int ok;

    ok = llm_m136_evidence_rollback(
        target,
        evidence);

    while (remove(target) == 0) {
    }
    while (remove(evidence) == 0) {
    }

    llm_selftest_report(
        "failed build evidence precedes rollback",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    const char *target =
        "OVMS_AGENT_TEST_M136_COMMIT.TMP";
    int ok;

    ok = llm_m136_commit_closes(target);

    while (remove(target) == 0) {
    }

    llm_selftest_report(
        "commit closes rollback capability",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    const char *plan =
        "OVMS_AGENT_TEST_M136_PLAN.TMP";
    const char *fresh =
        "OVMS_AGENT_TEST_M136_NEW.TMP";
    const char *backup =
        "OVMS_AGENT_TEST_M136_OLD.TMP";
    int ok;

    ok = llm_m136_consume_restore(
        plan,
        fresh,
        backup);

    while (remove(plan) == 0) {
    }
    while (remove(fresh) == 0) {
    }
    while (remove(backup) == 0) {
    }

    llm_selftest_report(
        "consume failure preserves committed outcome",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    const char *path =
        "OVMS_AGENT_TEST_M136_STATE.TMP";
    int ok;

    while (remove(path) == 0) {
    }

    ok = llm_m136_final_state(path);

    while (remove(path) == 0) {
    }

    llm_selftest_report(
        "execution state reflects final transaction",
        ok,
        &passed_count,
        &failed_count
    );
}
