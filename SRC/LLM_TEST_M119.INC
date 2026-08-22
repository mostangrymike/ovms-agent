{
    const char *path = "OVMS_AGENT_TEST_M119_WRITE_NEG.TMP";
    int ok;

    while (remove(path) == 0) {
    }

    ok = openai_m119_writer_negative(path);

    while (remove(path) == 0) {
    }

    openai_selftest_report(
        "writer rejects negative build status",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    const char *path = "OVMS_AGENT_TEST_M119_LOAD_NEG.TMP";
    int ok;

    while (remove(path) == 0) {
    }

    ok = openai_m119_loader_negative(path);

    while (remove(path) == 0) {
    }

    openai_selftest_report(
        "loader rejects negative build status",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    const char *path = "OVMS_AGENT_TEST_M119_ZERO.TMP";
    int ok;

    while (remove(path) == 0) {
    }

    ok = openai_m119_known_zero(path);

    while (remove(path) == 0) {
    }

    openai_selftest_report(
        "known zero build status accepted",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    const char *path = "OVMS_AGENT_TEST_M119_RELOAD.TMP";
    int ok;

    while (remove(path) == 0) {
    }

    ok = openai_m119_writer_reloads(path);

    while (remove(path) == 0) {
    }

    openai_selftest_report(
        "writer output reloads successfully",
        ok,
        &passed_count,
        &failed_count
    );
}
