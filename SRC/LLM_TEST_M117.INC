{
    const char *path =
        "OVMS_AGENT_TEST_M117_UNIQUE.TMP";
    int ok;

    while (remove(path) == 0) {
    }

    ok = llm_m117_tmp_is_unique(path);

    while (remove(path) == 0) {
    }

    llm_selftest_report(
        "production temporary path is unique",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    const char *path =
        "OVMS_AGENT_TEST_M117_TRUNCATION.TMP";
    int ok;

    ok = llm_m117_tmp_truncation(path);

    llm_selftest_report(
        "temporary path truncation rejected",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    const char *path =
        "OVMS_AGENT_TEST_M117_COLLISION.TMP";
    int ok;

    ok = llm_m117_tmp_no_collision(path);

    llm_selftest_report(
        "separate temporary paths do not collide",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    const char *path =
        "OVMS_AGENT_TEST_M117_ATOMIC.TMP";
    int ok;

    while (remove(path) == 0) {
    }

    ok = llm_m117_atomic_valid(path);

    while (remove(path) == 0) {
    }

    llm_selftest_report(
        "atomic production writer still valid",
        ok,
        &passed_count,
        &failed_count
    );
}
