{
    int ok;

    ok = openai_m150b_create_commit();

    openai_selftest_report(
        "M150B create commit retains file",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = openai_m150b_create_rollback();

    openai_selftest_report(
        "M150B rollback deletes created file",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = openai_m150b_existing_reject();

    openai_selftest_report(
        "M150B existing create target rejected",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = openai_m150b_mixed_rollback();

    openai_selftest_report(
        "M150B mixed transaction rolls back",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = openai_m150b_same_path_reject();

    openai_selftest_report(
        "M150B create path reuse rejected",
        ok,
        &passed_count,
        &failed_count
    );
}
