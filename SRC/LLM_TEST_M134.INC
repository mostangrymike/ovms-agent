{
    int ok;

    ok = llm_m134_reset_consolidated();

    llm_selftest_report(
        "state reset path is consolidated",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = llm_m134_models_distinct();

    llm_selftest_report(
        "load and save state models remain distinct",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = llm_m134_invalid_suppressed();

    llm_selftest_report(
        "invalid status output remains suppressed",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = llm_m134_final_audit();

    llm_selftest_report(
        "state subsystem final audit complete",
        ok,
        &passed_count,
        &failed_count
    );
}
