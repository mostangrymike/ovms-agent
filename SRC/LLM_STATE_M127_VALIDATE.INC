int openai_m127_hide_workflow(void)
{
    return strcmp(
        openai_persist_workflow(
            0,
            OPENAI_WORKFLOW_EXECUTE),
        "unavailable") == 0;
}

int openai_m127_hide_build(void)
{
    return strcmp(
        openai_persist_build(
            0,
            1,
            1),
        "unavailable") == 0;
}

int openai_m127_hide_rollback(void)
{
    return strcmp(
        openai_persist_rollback(
            0,
            OPENAI_ROLLBACK_SUCCEEDED),
        "unavailable") == 0;
}

int openai_m127_valid_details(void)
{
    return
        strcmp(
            openai_persist_workflow(
                1,
                OPENAI_WORKFLOW_EXECUTE),
            openai_workflow_name(
                OPENAI_WORKFLOW_EXECUTE)) == 0 &&
        strcmp(
            openai_persist_build(
                1,
                1,
                1),
            "success") == 0 &&
        strcmp(
            openai_persist_rollback(
                1,
                OPENAI_ROLLBACK_SUCCEEDED),
            openai_rollback_name(
                OPENAI_ROLLBACK_SUCCEEDED)) == 0;
}
