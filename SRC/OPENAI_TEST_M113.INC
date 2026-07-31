{
    const char *path =
        "OVMS_AGENT_TEST_M113_SIGNED.TMP";
    int ok;

    while (remove(path) == 0) {
    }

    ok = openai_m113_signed_workflow(path);

    while (remove(path) == 0) {
    }

    openai_selftest_report(
        "signed workflow value rejected",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    const char *path =
        "OVMS_AGENT_TEST_M113_ZERO_PREFIX.TMP";
    int ok;

    while (remove(path) == 0) {
    }

    ok = openai_m113_zero_workflow(path);

    while (remove(path) == 0) {
    }

    openai_selftest_report(
        "leading-zero workflow rejected",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    const char *path =
        "OVMS_AGENT_TEST_M113_SPACED.TMP";
    int ok;

    while (remove(path) == 0) {
    }

    ok = openai_m113_spaced_value(path);

    while (remove(path) == 0) {
    }

    openai_selftest_report(
        "spaced state value rejected",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    const char *path =
        "OVMS_AGENT_TEST_M113_ZERO_VALUES.TMP";
    int ok;

    while (remove(path) == 0) {
    }

    ok = openai_m113_zero_values(path);

    while (remove(path) == 0) {
    }

    openai_selftest_report(
        "canonical zero values accepted",
        ok,
        &passed_count,
        &failed_count
    );
}
