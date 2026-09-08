extern int llm_plan_approved;
extern unsigned long llm_approved_hash;
extern int llm_approval_invalidated;

static void llm_m310_invalidate_plan_approval(void)
{
    if (!llm_plan_approved) {
        return;
    }

    llm_plan_approved = 0;
    llm_approved_hash = 0UL;
    llm_approval_invalidated = 1;
}

int llm_plan_is_current(int verbose)
{
    int current;

    current = llm_plan_file_current(
        LLM_PLAN_FILE,
        verbose);

    if (!current) {
        llm_m310_invalidate_plan_approval();
    }

    return current;
}
