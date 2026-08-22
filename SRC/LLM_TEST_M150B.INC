{
    int ok;

    ok = llm_m150b_create_commit();

    llm_selftest_report(
        "M150B create commit retains file",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = llm_m150b_create_rollback();

    llm_selftest_report(
        "M150B rollback deletes created file",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = llm_m150b_existing_reject();

    llm_selftest_report(
        "M150B existing create target rejected",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = llm_m150b_mixed_rollback();

    llm_selftest_report(
        "M150B mixed transaction rolls back",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = llm_m150b_same_path_reject();

    llm_selftest_report(
        "M150B create path reuse rejected",
        ok,
        &passed_count,
        &failed_count
    );
}
