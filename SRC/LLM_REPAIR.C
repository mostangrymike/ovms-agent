#include "LLM_REPAIR.H"

#define openai_repair_plan llm_repair_plan
#include "LLM_REPAIR_CORE.INC"
#undef openai_repair_plan

void openai_repair_plan(agent_state *state)
{
    llm_repair_plan(state);
}
