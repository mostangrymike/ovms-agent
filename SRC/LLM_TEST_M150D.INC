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

{
    int llm_m303_exec_policy(void);
    int ok;

    ok = llm_m303_exec_policy();

    llm_selftest_report(
        "M303 read-only blocks saved-plan execution",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int llm_m303_exec_write_gate(void);
    int ok;

    ok = llm_m303_exec_write_gate();

    llm_selftest_report(
        "M303 write gate blocks saved-plan execution",
        ok,
        &passed_count,
        &failed_count
    );
}

{
    int llm_m303_exec_dcl_gate(void);
    int ok;

    ok = llm_m303_exec_dcl_gate();

    llm_selftest_report(
        "M303 DCL gate blocks saved-plan execution",
        ok,
        &passed_count,
        &failed_count
    );
}
