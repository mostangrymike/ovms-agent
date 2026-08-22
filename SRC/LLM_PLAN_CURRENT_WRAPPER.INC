int llm_plan_is_current(int verbose)
{
    return llm_plan_file_current(
        LLM_PLAN_FILE,
        verbose);
}
