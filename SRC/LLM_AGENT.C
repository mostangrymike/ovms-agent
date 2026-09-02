#include <stdlib.h>

#include "llm_internal.h"
#include "LLM_AGENT_LIMITS.H"
#include "LLM_PROMPTS.H"
#include "LLM_PROJECT_MAP.H"

#ifndef LLM_PLAN_MAX_TURNS
#define LLM_PLAN_MAX_TURNS 24
#endif


static void llm_agent_instr(agent_state *state,
                            const char *goal,
                            int allow_write,
                            int build_after_write,
                            int workflow)
{
    char *instr_goal;
    char *project_goal;
    char *model_goal;

    instr_goal = (char *)malloc((size_t)LLM_AGENT_INSTR_GOAL_MAX);
    project_goal = (char *)malloc((size_t)LLM_AGENT_PROJECT_GOAL_MAX);
    model_goal = (char *)malloc((size_t)LLM_AGENT_MODEL_GOAL_MAX);

    if (instr_goal == NULL || project_goal == NULL || model_goal == NULL) {
        free(instr_goal);
        free(project_goal);
        free(model_goal);
        (void)puts("Unable to allocate agent context buffers.");
        return;
    }

    if (!llm_instr_compose(
            state,
            goal,
            instr_goal,
            (size_t)LLM_AGENT_INSTR_GOAL_MAX)) {
        free(instr_goal);
        free(project_goal);
        free(model_goal);
        (void)puts("Unable to compose project instructions.");
        return;
    }

    if (!llm_project_compose(
            state,
            instr_goal,
            project_goal,
            (size_t)LLM_AGENT_PROJECT_GOAL_MAX)) {
        free(instr_goal);
        free(project_goal);
        free(model_goal);
        (void)puts("Unable to compose repository map.");
        return;
    }

    if (!llm_git_compose(
            state,
            project_goal,
            model_goal,
            (size_t)LLM_AGENT_MODEL_GOAL_MAX)) {
        free(instr_goal);
        free(project_goal);
        free(model_goal);
        (void)puts("Unable to compose Git context.");
        return;
    }

    llm_agent_mode(
        state,
        model_goal,
        allow_write,
        build_after_write,
        workflow
    );

    free(instr_goal);
    free(project_goal);
    free(model_goal);
}

void llm_agent(agent_state *state, const char *goal)
{
    llm_agent_instr(
        state, goal, 0, 0, LLM_WORKFLOW_AGENT
    );
}

void llm_agent_plan(agent_state *state, const char *goal)
{
    llm_agent_instr(
        state, goal, 0, 0, LLM_WORKFLOW_PLAN
    );
}

void llm_agent_write(agent_state *state, const char *goal)
{
    llm_agent_instr(
        state, goal, 1, 0, LLM_WORKFLOW_WRITE
    );
}

void llm_agent_fix(agent_state *state, const char *goal)
{
    llm_agent_instr(
        state, goal, 1, 1, LLM_WORKFLOW_FIX
    );
}
