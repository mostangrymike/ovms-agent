{
    int ok;

    ok = openai_m134_reset_consolidated();

    openai_selftest_report(
        "state reset path is consolidated",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = openai_m134_models_distinct();

    openai_selftest_report(
        "load and save state models remain distinct",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = openai_m134_invalid_suppressed();

    openai_selftest_report(
        "invalid status output remains suppressed",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = openai_m134_final_audit();

    openai_selftest_report(
        "state subsystem final audit complete",
        ok,
        &passed_count,
        &failed_count
    );
}
