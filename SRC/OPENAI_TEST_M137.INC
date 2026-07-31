{
    const char *plan =
        "OVMS_AGENT_TEST_M137_PLAN.TMP";
    const char *fresh =
        "OVMS_AGENT_TEST_M137_NEW.TMP";
    const char *backup =
        "OVMS_AGENT_TEST_M137_OLD.TMP";
    int ok;

    ok = openai_m137_success_cleanup(
        plan,
        fresh,
        backup);

    while (remove(plan) == 0) {
    }
    while (remove(fresh) == 0) {
    }
    while (remove(backup) == 0) {
    }

    openai_selftest_report(
        "successful consumption cleans temporary files",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    const char *plan =
        "OVMS_AGENT_TEST_M137_FAIL_PLAN.TMP";
    const char *fresh =
        "OVMS_AGENT_TEST_M137_FAIL_NEW.TMP";
    const char *backup =
        "OVMS_AGENT_TEST_M137_FAIL_OLD.TMP";
    int ok;

    ok = openai_m137_failure_restore(
        plan,
        fresh,
        backup);

    while (remove(plan) == 0) {
    }
    while (remove(fresh) == 0) {
    }
    while (remove(backup) == 0) {
    }

    openai_selftest_report(
        "failed consumption restores approved plan",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    const char *plan =
        "OVMS_AGENT_TEST_M137_CONSUMED.TMP";
    int ok;

    ok = openai_m137_consumed_rejected(plan);

    while (remove(plan) == 0) {
    }

    openai_selftest_report(
        "consumed plan cannot execute again",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    const char *plan =
        "OVMS_AGENT_TEST_M137_RECOVER.TMP";
    const char *fresh =
        "OVMS_AGENT_TEST_M137_RECOVER_NEW.TMP";
    const char *backup =
        "OVMS_AGENT_TEST_M137_RECOVER_OLD.TMP";
    int ok;

    ok = openai_m137_artifact_recovery(
        plan,
        fresh,
        backup);

    while (remove(plan) == 0) {
    }
    while (remove(fresh) == 0) {
    }
    while (remove(backup) == 0) {
    }

    openai_selftest_report(
        "stale-plan checks ignore recovery artifacts",
        ok,
        &passed_count,
        &failed_count
    );
}
