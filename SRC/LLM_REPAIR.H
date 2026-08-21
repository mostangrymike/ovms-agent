#ifndef LLM_REPAIR_H
#define LLM_REPAIR_H

#include "agent.h"

#ifdef __cplusplus
extern "C" {
#endif

void llm_repair_plan(agent_state *state);
void openai_repair_plan(agent_state *state);

#ifdef __cplusplus
}
#endif

#endif /* LLM_REPAIR_H */
