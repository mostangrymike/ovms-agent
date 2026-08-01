{
    int ok;

    ok = openai_m150d_success();

    openai_selftest_report(
        "M150D approved create executes",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = openai_m150d_rollback();

    openai_selftest_report(
        "M150D failed build removes create",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = openai_m150d_tamper();

    openai_selftest_report(
        "M150D checksum tamper blocks create",
        ok,
        &passed_count,
        &failed_count
    );
}
