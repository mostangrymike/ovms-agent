#include "openai_internal.h"
#include "openai_prompts.h"

#ifndef OPENAI_PLAN_MAX_TURNS
#define OPENAI_PLAN_MAX_TURNS 24
#endif


void openai_agent(agent_state *state, const char *goal)
{
    openai_agent_mode(state, goal, 0, 0, OPENAI_WORKFLOW_AGENT);
}

void openai_agent_plan(agent_state *state, const char *goal)
{
    openai_agent_mode(state, goal, 0, 0, OPENAI_WORKFLOW_PLAN);
}

void openai_agent_write(agent_state *state, const char *goal)
{
    openai_agent_mode(state, goal, 1, 0, OPENAI_WORKFLOW_WRITE);
}

void openai_agent_fix(agent_state *state, const char *goal)
{
    openai_agent_mode(state, goal, 1, 1, OPENAI_WORKFLOW_FIX);
}

