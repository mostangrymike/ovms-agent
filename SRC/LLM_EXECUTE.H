#ifndef LLM_EXECUTE_H
#define LLM_EXECUTE_H

#include "agent.h"

void llm_plan_execute(agent_state *state);
void llm_plan_execute_dry(agent_state *state);

int llm_execute_count_ops(
    const char *plan_text,
    unsigned int *operation_count_out);

/* Temporary M267 source compatibility for remaining live consumers. */
#define openai_plan_execute llm_plan_execute
#define openai_execute_count_ops llm_execute_count_ops

#endif /* LLM_EXECUTE_H */
