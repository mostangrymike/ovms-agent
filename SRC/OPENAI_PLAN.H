#ifndef OPENAI_PLAN_H
#define OPENAI_PLAN_H

#include "agent.h"

void openai_agent_plan(agent_state *state, const char *goal);

int openai_plan_save(const char *goal,
                     const char *plan_text);
void openai_plan_show(void);
int openai_plan_is_current(int verbose);
void openai_plan_validate(void);
void openai_plan_clear(void);

#endif /* OPENAI_PLAN_H */
