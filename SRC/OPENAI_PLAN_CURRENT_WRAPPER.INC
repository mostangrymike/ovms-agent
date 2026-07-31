int openai_plan_is_current(int verbose)
{
    return openai_plan_file_current(
        OPENAI_PLAN_FILE,
        verbose);
}
