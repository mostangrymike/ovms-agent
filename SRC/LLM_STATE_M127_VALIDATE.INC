int llm_m127_hide_workflow(void)
{
    return strcmp(
        llm_persist_workflow(
            0,
            LLM_WORKFLOW_EXECUTE),
        "unavailable") == 0;
}

int llm_m127_hide_build(void)
{
    return strcmp(
        llm_persist_build(
            0,
            1,
            1),
        "unavailable") == 0;
}

int llm_m127_hide_rollback(void)
{
    return strcmp(
        llm_persist_rollback(
            0,
            LLM_ROLLBACK_SUCCEEDED),
        "unavailable") == 0;
}

int llm_m127_valid_details(void)
{
    return
        strcmp(
            llm_persist_workflow(
                1,
                LLM_WORKFLOW_EXECUTE),
            llm_workflow_name(
                LLM_WORKFLOW_EXECUTE)) == 0 &&
        strcmp(
            llm_persist_build(
                1,
                1,
                1),
            "success") == 0 &&
        strcmp(
            llm_persist_rollback(
                1,
                LLM_ROLLBACK_SUCCEEDED),
            llm_rollback_name(
                LLM_ROLLBACK_SUCCEEDED)) == 0;
}
