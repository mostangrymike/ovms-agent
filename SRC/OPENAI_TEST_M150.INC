{
    int ok;

    ok = openai_m150_create_valid();

    openai_selftest_report(
        "M150 create_file syntax accepted",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = openai_m150_create_old_reject();

    openai_selftest_report(
        "M150 create_file old_text rejected",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = openai_m150_create_missing_new();

    openai_selftest_report(
        "M150 create_file requires new_text",
        ok,
        &passed_count,
        &failed_count
    );
}
