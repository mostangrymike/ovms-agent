static int openai_m115_writer_rejects(
    const char *path,
    int workflow,
    int build_known,
    int build_status,
    int rollback)
{
    return !openai_state_save_to(
        path,
        workflow,
        build_known,
        build_status,
        rollback);
}

int openai_m115_bad_workflow(
    const char *path)
{
    return openai_m115_writer_rejects(
        path,
        OPENAI_WORKFLOW_EXECUTE + 1,
        1,
        1,
        OPENAI_ROLLBACK_NONE);
}

int openai_m115_bad_build_flag(
    const char *path)
{
    return openai_m115_writer_rejects(
        path,
        OPENAI_WORKFLOW_EXECUTE,
        2,
        0,
        OPENAI_ROLLBACK_NONE);
}

int openai_m115_bad_build_state(
    const char *path)
{
    return openai_m115_writer_rejects(
        path,
        OPENAI_WORKFLOW_EXECUTE,
        0,
        2,
        OPENAI_ROLLBACK_NONE);
}

int openai_m115_bad_rollback(
    const char *path)
{
    return openai_m115_writer_rejects(
        path,
        OPENAI_WORKFLOW_EXECUTE,
        1,
        1,
        OPENAI_ROLLBACK_DECLINED + 1);
}
