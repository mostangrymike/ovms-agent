{
    const char *path =
        "OVMS_AGENT_TEST_M138_BODY.TMP";
    int ok;

    while (remove(path) == 0) {
    }
    while (remove("OVMS_AGENT_TEST_M138_BODY.TMP.CHK") == 0) {
    }

    ok = openai_m138_body_tamper(path);

    while (remove(path) == 0) {
    }
    while (remove("OVMS_AGENT_TEST_M138_BODY.TMP.CHK") == 0) {
    }

    openai_selftest_report(
        "operation-body tampering invalidates plan",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    const char *path =
        "OVMS_AGENT_TEST_M138_META.TMP";
    int ok;

    while (remove(path) == 0) {
    }
    while (remove("OVMS_AGENT_TEST_M138_META.TMP.CHK") == 0) {
    }

    ok = openai_m138_meta_tamper(path);

    while (remove(path) == 0) {
    }
    while (remove("OVMS_AGENT_TEST_M138_META.TMP.CHK") == 0) {
    }

    openai_selftest_report(
        "metadata tampering invalidates plan",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    const char *path =
        "OVMS_AGENT_TEST_M138_PARTIAL.TMP";
    int ok;

    while (remove(path) == 0) {
    }
    while (remove("OVMS_AGENT_TEST_M138_PARTIAL.TMP.CHK") == 0) {
    }

    ok = openai_m138_partial_reject(path);

    while (remove(path) == 0) {
    }
    while (remove("OVMS_AGENT_TEST_M138_PARTIAL.TMP.CHK") == 0) {
    }

    openai_selftest_report(
        "partial plan cannot become executable",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    const char *path = "OVMS_AGENT_PLAN.TXT";
    int ok;

    ok = openai_m138_save_preserves(path);

    openai_selftest_report(
        "failed plan replacement preserves prior plan",
        ok,
        &passed_count,
        &failed_count
    );
}
