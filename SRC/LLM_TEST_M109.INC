{
    const char *path = "OVMS_AGENT_TEST_M109_SAVE.TMP";
    char *text;
    int ok;

    while (remove(path) == 0) {
    }

    ok = llm_m109_state_writer(path);
    text = llm_read_text_file(path);
    ok =
        ok &&
        text != NULL &&
        strstr(text, "format=1") != NULL &&
        strstr(text, "last_workflow=13") != NULL &&
        strstr(text, "last_build_known=1") != NULL &&
        strstr(text, "last_build_status=2") != NULL &&
        strstr(text, "last_rollback=2") != NULL;
    free(text);

    while (remove(path) == 0) {
    }

    llm_selftest_report(
        "production state serializer tested",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    const char *path = "OVMS_AGENT_TEST_M109_LOAD.TMP";
    int ok;

    while (remove(path) == 0) {
    }

    ok =
        llm_m109_state_writer(path) &&
        llm_m109_state_reader(path);

    while (remove(path) == 0) {
    }

    llm_selftest_report(
        "production execute state reload tested",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    const char *path = "OVMS_AGENT_TEST_M109_BAD_WORKFLOW.TMP";
    int ok;

    while (remove(path) == 0) {
    }

    ok = llm_m109_invalid_workflow(path);

    while (remove(path) == 0) {
    }

    llm_selftest_report(
        "invalid workflow state rejected",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    const char *path = "OVMS_AGENT_TEST_M109_INCOMPLETE.TMP";
    int ok;

    while (remove(path) == 0) {
    }

    ok = llm_m109_incomplete_state(path);

    while (remove(path) == 0) {
    }

    llm_selftest_report(
        "incomplete state file rejected",
        ok,
        &passed_count,
        &failed_count
    );
}
