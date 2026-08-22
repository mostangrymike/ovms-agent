{
    int ok;

    ok = llm_m150_create_valid();

    llm_selftest_report(
        "M150 create_file syntax accepted",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = llm_m150_create_old_reject();

    llm_selftest_report(
        "M150 create_file old_text rejected",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = llm_m150_create_missing_new();

    llm_selftest_report(
        "M150 create_file requires new_text",
        ok,
        &passed_count,
        &failed_count
    );
}
