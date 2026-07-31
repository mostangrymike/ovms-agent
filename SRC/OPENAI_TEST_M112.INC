{
    const char *path =
        "OVMS_AGENT_TEST_M112_BIG_WORKFLOW.TMP";
    int ok;

    while (remove(path) == 0) {
    }

    ok = openai_m112_big_workflow(path);

    while (remove(path) == 0) {
    }

    openai_selftest_report(
        "oversized workflow record rejected",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    const char *path =
        "OVMS_AGENT_TEST_M112_BIG_UNKNOWN.TMP";
    int ok;

    while (remove(path) == 0) {
    }

    ok = openai_m112_big_unknown(path);

    while (remove(path) == 0) {
    }

    openai_selftest_report(
        "oversized unknown record rejected",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    const char *path =
        "OVMS_AGENT_TEST_M112_UNTERM_MIDDLE.TMP";
    int ok;

    while (remove(path) == 0) {
    }

    ok = openai_m112_unterm_middle(path);

    while (remove(path) == 0) {
    }

    openai_selftest_report(
        "unterminated nonfinal record rejected",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    const char *path =
        "OVMS_AGENT_TEST_M112_UNTERM_FINAL.TMP";
    int ok;

    while (remove(path) == 0) {
    }

    ok = openai_m112_unterm_final(path);

    while (remove(path) == 0) {
    }

    openai_selftest_report(
        "unterminated final rollback accepted",
        ok,
        &passed_count,
        &failed_count
    );
}
