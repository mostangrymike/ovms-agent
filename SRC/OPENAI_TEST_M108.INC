{
    const char *path = "OVMS_AGENT_TEST_M108_BUILD.TMP";
    char *text;
    int ok;

    while (remove(path) == 0) {
    }

    ok = openai_m108_build_writer(path);
    text = openai_read_text_file(path);
    ok =
        ok &&
        text != NULL &&
        strstr(text, "M108 PRODUCTION BUILD EVIDENCE") != NULL;
    free(text);

    while (remove(path) == 0) {
    }

    openai_selftest_report(
        "production build evidence writer tested",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    const char *path = "OVMS_AGENT_TEST_M108_OPS.TMP";
    char *text;
    int ok;

    while (remove(path) == 0) {
    }

    ok = openai_m108_ops_writer(path);
    text = openai_read_text_file(path);
    ok =
        ok &&
        text != NULL &&
        strstr(text, "operation_count=1") != NULL &&
        strstr(text, "path=SRC/M108.C") != NULL &&
        strstr(text, "OLD") != NULL &&
        strstr(text, "NEW") != NULL;
    free(text);

    while (remove(path) == 0) {
    }

    openai_selftest_report(
        "production operation evidence writer tested",
        ok,
        &passed_count,
        &failed_count
    );
}

openai_selftest_report(
    "evidence writer rejects null path",
    openai_m108_null_path(),
    &passed_count,
    &failed_count
);

{
    const char *path = "OVMS_AGENT_TEST_M108_NULL_OPS.TMP";

    while (remove(path) == 0) {
    }

    openai_selftest_report(
        "evidence writer rejects null operations",
        openai_m108_null_ops(path),
        &passed_count,
        &failed_count
    );

    while (remove(path) == 0) {
    }
}
