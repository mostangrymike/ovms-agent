{
    const char *path =
        "OVMS_AGENT_TEST_M123_NEWEST.TMP";
    int ok;

    while (remove(path) == 0) {
    }

    ok = openai_m123_newest_preferred(path);

    while (remove(path) == 0) {
    }

    openai_selftest_report(
        "valid newest state preferred",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    const char *path =
        "OVMS_AGENT_TEST_M123_FALLBACK.TMP";
    int ok;

    while (remove(path) == 0) {
    }

    ok = openai_m123_fallback_prior(path);

    while (remove(path) == 0) {
    }

    openai_selftest_report(
        "invalid newest falls back to prior",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    const char *path =
        "OVMS_AGENT_TEST_M123_INVALID.TMP";
    int ok;

    while (remove(path) == 0) {
    }

    ok = openai_m123_both_invalid(path);

    while (remove(path) == 0) {
    }

    openai_selftest_report(
        "invalid prior version also rejected",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    const char *path =
        "OVMS_AGENT_TEST_M123_READONLY.TMP";
    int ok;

    while (remove(path) == 0) {
    }

    ok = openai_m123_read_only(path);

    while (remove(path) == 0) {
    }

    openai_selftest_report(
        "recovery load does not modify versions",
        ok,
        &passed_count,
        &failed_count
    );
}
