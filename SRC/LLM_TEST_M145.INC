{
    int ok;

    ok = openai_m145_fresh_unapproved();

    openai_selftest_report(
        "fresh session starts unapproved",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    const char *path =
        "OVMS_AGENT_TEST_M145_ACTIVE.TMP";
    int ok;

    ok = openai_m145_active_no_restore(path);

    openai_selftest_report(
        "active plan does not restore approval",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    const char *path =
        "OVMS_AGENT_TEST_M145_CONSUMED.TMP";
    int ok;

    ok = openai_m145_consumed_no_restore(path);

    openai_selftest_report(
        "consumed plan does not restore approval",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = openai_m145_new_no_restore(
        "OVMS_AGENT_TEST_M145_NEW_PLAN.TMP",
        "OVMS_AGENT_TEST_M145_NEW.TMP",
        "OVMS_AGENT_TEST_M145_NEW_OLD.TMP");

    openai_selftest_report(
        "NEW recovery does not restore approval",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = openai_m145_old_no_restore(
        "OVMS_AGENT_TEST_M145_OLD_PLAN.TMP",
        "OVMS_AGENT_TEST_M145_OLD_NEW.TMP",
        "OVMS_AGENT_TEST_M145_OLD.TMP");

    openai_selftest_report(
        "OLD recovery does not restore approval",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    const char *path =
        "OVMS_AGENT_TEST_M145_STATE.TMP";
    int ok;

    ok = openai_m145_state_no_restore(path);

    openai_selftest_report(
        "workflow state does not restore approval",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = openai_m145_logs_no_restore();

    openai_selftest_report(
        "logs do not restore approval",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    const char *path =
        "OVMS_AGENT_TEST_M145_RESET.TMP";
    int ok;

    ok = openai_m145_reset_authority(path);

    openai_selftest_report(
        "session reset clears all approval authority",
        ok,
        &passed_count,
        &failed_count
    );
}
