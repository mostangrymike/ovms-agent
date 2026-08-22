static int llm_m115_writer_rejects(
    const char *path,
    int workflow,
    int build_known,
    int build_status,
    int rollback)
{
    return !llm_state_save_to(
        path,
        workflow,
        build_known,
        build_status,
        rollback);
}

int llm_m115_bad_workflow(
    const char *path)
{
    return llm_m115_writer_rejects(
        path,
        LLM_WORKFLOW_EXECUTE + 1,
        1,
        1,
        LLM_ROLLBACK_NONE);
}

int llm_m115_bad_build_flag(
    const char *path)
{
    return llm_m115_writer_rejects(
        path,
        LLM_WORKFLOW_EXECUTE,
        2,
        0,
        LLM_ROLLBACK_NONE);
}

int llm_m115_bad_build_state(
    const char *path)
{
    return llm_m115_writer_rejects(
        path,
        LLM_WORKFLOW_EXECUTE,
        0,
        2,
        LLM_ROLLBACK_NONE);
}

int llm_m115_bad_rollback(
    const char *path)
{
    return llm_m115_writer_rejects(
        path,
        LLM_WORKFLOW_EXECUTE,
        1,
        1,
        LLM_ROLLBACK_DECLINED + 1);
}
