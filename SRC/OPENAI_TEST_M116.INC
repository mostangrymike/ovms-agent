{
    const char *path =
        "OVMS_AGENT_TEST_M116_ATOMIC.TMP";
    int ok;

    while (remove(path) == 0) {
    }

    ok = openai_m116_atomic_create(path);

    while (remove(path) == 0) {
    }

    openai_selftest_report(
        "atomic writer creates valid state",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    const char *path =
        "OVMS_AGENT_TEST_M116_TMP_FAIL.TMP";
    int ok;

    while (remove(path) == 0) {
    }

    ok = openai_m116_tmp_fail_keeps(path);

    while (remove(path) == 0) {
    }

    openai_selftest_report(
        "failed temporary write preserves state",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    const char *path =
        "OVMS_AGENT_TEST_M116_BAD_VALUES.TMP";
    int ok;

    while (remove(path) == 0) {
    }

    ok = openai_m116_bad_values_keep(path);

    while (remove(path) == 0) {
    }

    openai_selftest_report(
        "invalid values preserve existing state",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    const char *path =
        "OVMS_AGENT_TEST_M116_CLEAN_TMP.TMP";
    int ok;

    while (remove(path) == 0) {
    }

    ok = openai_m116_tmp_versions_clean(path);

    while (remove(path) == 0) {
    }

    openai_selftest_report(
        "temporary state versions cleaned",
        ok,
        &passed_count,
        &failed_count
    );
}
