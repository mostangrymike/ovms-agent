#ifndef LLM_PLAN_H
#define LLM_PLAN_H

#include "agent.h"

void llm_agent_plan(agent_state *state, const char *goal);

/* These three still have deliberate *_legacy remaps inside LLM_PLAN.C. */
int openai_plan_save(const char *goal,
                     const char *plan_text);
void openai_plan_show(void);
void openai_plan_clear(void);

int llm_plan_is_current(int verbose);
void llm_plan_validate(void);

/* Temporary M267 source compatibility for remaining live consumers. */
#define openai_agent_plan llm_agent_plan
#define openai_plan_is_current llm_plan_is_current
#define openai_plan_validate llm_plan_validate

#endif /* LLM_PLAN_H */
