#ifndef LLM_PLAN_H
#define LLM_PLAN_H

#include "agent.h"

void llm_agent_plan(agent_state *state, const char *goal);

int openai_plan_save(const char *goal,
                     const char *plan_text);
void openai_plan_show(void);
int openai_plan_is_current(int verbose);
void openai_plan_validate(void);
void openai_plan_clear(void);

/* Temporary M267 source compatibility for remaining live consumers. */
#define openai_agent_plan llm_agent_plan

#endif /* LLM_PLAN_H */
