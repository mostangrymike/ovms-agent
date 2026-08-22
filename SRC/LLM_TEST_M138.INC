int llm_m251_fence_old(void);
int llm_m251_fence_new(void);
int llm_m251_size_old(void);
int llm_m251_size_new(void);
int llm_m251_payload_ok_test(void);
int llm_m251_narr_ok(void);

{
    const char *path =
        "OVMS_AGENT_TEST_M138_BODY.TMP";
    int ok;

    while (remove(path) == 0) {
    }
    while (remove("OVMS_AGENT_TEST_M138_BODY.TMP.CHK") == 0) {
    }

    ok = llm_m138_body_tamper(path);

    while (remove(path) == 0) {
    }
    while (remove("OVMS_AGENT_TEST_M138_BODY.TMP.CHK") == 0) {
    }

    llm_selftest_report(
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

    ok = llm_m138_meta_tamper(path);

    while (remove(path) == 0) {
    }
    while (remove("OVMS_AGENT_TEST_M138_META.TMP.CHK") == 0) {
    }

    llm_selftest_report(
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

    ok = llm_m138_partial_reject(path);

    while (remove(path) == 0) {
    }
    while (remove("OVMS_AGENT_TEST_M138_PARTIAL.TMP.CHK") == 0) {
    }

    llm_selftest_report(
        "partial plan cannot become executable",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    const char *path = "OVMS_AGENT_PLAN.TXT";
    int ok;

    ok = llm_m138_save_preserves(path);

    llm_selftest_report(
        "failed plan replacement preserves prior plan",
        ok,
        &passed_count,
        &failed_count
    );
}
{
    int ok;

    ok = llm_m195_normalize_one();

    llm_selftest_report(
        "M195 incorrect count normalized",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = llm_m195_normalize_two();

    llm_selftest_report(
        "M195 two operations counted",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = llm_m195_normalize_correct();

    llm_selftest_report(
        "M195 correct count preserved",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = llm_m195_norm_inspect();

    llm_selftest_report(
        "M195 inspection plan preserved",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = llm_m195_reject_missing();

    llm_selftest_report(
        "M195 missing count rejected",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = llm_m195_reject_duplicate();

    llm_selftest_report(
        "M195 duplicate count rejected",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = llm_m195_reject_markers();

    llm_selftest_report(
        "M195 invalid markers rejected",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = llm_m195_reject_early_begin();

    llm_selftest_report(
        "M195 early operation before count rejected",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = llm_m195_failure_preserves();

    llm_selftest_report(
        "M195 rejection preserves saved plan",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = llm_m251_fence_old();

    llm_selftest_report(
        "M251 fence in OLD payload rejected",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = llm_m251_fence_new();

    llm_selftest_report(
        "M251 fence in NEW payload rejected",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = llm_m251_size_old();

    llm_selftest_report(
        "M251 oversized OLD payload rejected",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = llm_m251_size_new();

    llm_selftest_report(
        "M251 oversized NEW payload rejected",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = llm_m251_payload_ok_test();

    llm_selftest_report(
        "M251 valid bounded payload accepted",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = llm_m251_narr_ok();

    llm_selftest_report(
        "M251 narrative fences allowed",
        ok,
        &passed_count,
        &failed_count
    );
}
