#ifndef LLM_PLAN_H
#define LLM_PLAN_H

#include "agent.h"

void llm_agent_plan(agent_state *state, const char *goal);

int llm_plan_save(const char *goal,
                     const char *plan_text);
void llm_plan_show(void);
int llm_plan_is_current(int verbose);
void llm_plan_validate(void);
void llm_plan_clear(void);

/* Temporary M267 source compatibility for remaining live consumers. */

#endif /* LLM_PLAN_H */
