#ifndef LLM_PLAN_M310_H
#define LLM_PLAN_M310_H

extern int llm_plan_approved;
extern unsigned long llm_approved_hash;
extern int llm_approval_invalidated;

static void llm_m310_plan_reject(void)
{
    if (llm_plan_approved) {
        llm_plan_approved = 0;
        llm_approved_hash = 0UL;
        llm_approval_invalidated = 1;
    }
}

static int llm_m310_plan_save(
    const char *goal,
    const char *plan_text)
{
    int result;

    result = llm_plan_save(goal, plan_text);

    if (!result) {
        llm_m310_plan_reject();
    }

    return result;
}

#undef llm_plan_save
#define llm_plan_save llm_m310_plan_save

#endif /* LLM_PLAN_M310_H */
