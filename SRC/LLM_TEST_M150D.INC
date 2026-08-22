{
    int ok;

    ok = llm_m150d_success();

    llm_selftest_report(
        "M150D approved create executes",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = llm_m150d_rollback();

    llm_selftest_report(
        "M150D failed build removes create",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int ok;

    ok = llm_m150d_tamper();

    llm_selftest_report(
        "M150D checksum tamper blocks create",
        ok,
        &passed_count,
        &failed_count
    );
}
