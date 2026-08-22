{
    const char *path =
        "OVMS_AGENT_TEST_M122_ZERO.TMP";
    int ok;

    while (remove(path) == 0) {
    }

    ok = openai_m122_zero_is_newest(path);

    while (remove(path) == 0) {
    }

    openai_selftest_report(
        "version zero selects newest state",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    const char *path =
        "OVMS_AGENT_TEST_M122_PRIOR.TMP";
    int ok;

    while (remove(path) == 0) {
    }

    ok = openai_m122_minus_one_prior(path);

    while (remove(path) == 0) {
    }

    openai_selftest_report(
        "minus-one selects prior state",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    const char *path =
        "OVMS_AGENT_TEST_M122_MISSING.TMP";
    int ok;

    while (remove(path) == 0) {
    }

    ok = openai_m122_missing_prior_fails(path);

    while (remove(path) == 0) {
    }

    openai_selftest_report(
        "missing prior version rejected",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    const char *path =
        "OVMS_AGENT_TEST_M122_READONLY.TMP";
    int ok;

    while (remove(path) == 0) {
    }

    ok = openai_m122_probe_read_only(path);

    while (remove(path) == 0) {
    }

    openai_selftest_report(
        "version probe leaves files unchanged",
        ok,
        &passed_count,
        &failed_count
    );
}
