{
    const char *path =
        "OVMS_AGENT_TEST_M114_UNKNOWN_NONZERO.TMP";
    int ok;

    while (remove(path) == 0) {
    }

    ok = openai_m114_unknown_nonzero(path);

    while (remove(path) == 0) {
    }

    openai_selftest_report(
        "unknown build with status rejected",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    const char *path =
        "OVMS_AGENT_TEST_M114_UNKNOWN_ZERO.TMP";
    int ok;

    while (remove(path) == 0) {
    }

    ok = openai_m114_unknown_zero(path);

    while (remove(path) == 0) {
    }

    openai_selftest_report(
        "unknown build with zero accepted",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    const char *path =
        "OVMS_AGENT_TEST_M114_KNOWN_SUCCESS.TMP";
    int ok;

    while (remove(path) == 0) {
    }

    ok = openai_m114_known_success(path);

    while (remove(path) == 0) {
    }

    openai_selftest_report(
        "known successful build accepted",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    const char *path =
        "OVMS_AGENT_TEST_M114_KNOWN_FAILURE.TMP";
    int ok;

    while (remove(path) == 0) {
    }

    ok = openai_m114_known_failure(path);

    while (remove(path) == 0) {
    }

    openai_selftest_report(
        "known failed build accepted",
        ok,
        &passed_count,
        &failed_count
    );
}
