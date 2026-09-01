/* M291 ordinary AGENT/WRITE uses one atomic existing-file write surface. */
int llm_auto_atomic_write(void)
{
    return llm_auto_cur_workflow == LLM_WORKFLOW_WRITE;
}
