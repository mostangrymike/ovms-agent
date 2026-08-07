#include "openai_internal.h"
#include "openai_prompts.h"

#ifndef OPENAI_PLAN_MAX_TURNS
#define OPENAI_PLAN_MAX_TURNS 24
#endif


static void openai_agent_instr(agent_state *state,
                               const char *goal,
                               int allow_write,
                               int build_after_write,
                               int workflow)
{
    char instr_goal[12288];
    char project_goal[24576];
    char model_goal[32768];

    if (!openai_instr_compose(
            state, goal, instr_goal, sizeof(instr_goal))) {
        (void)puts("Unable to compose project instructions.");
        return;
    }

    if (!openai_project_compose(
            state, instr_goal, project_goal, sizeof(project_goal))) {
        (void)puts("Unable to compose repository map.");
        return;
    }

    if (!openai_git_compose(
            state, project_goal, model_goal, sizeof(model_goal))) {
        (void)puts("Unable to compose Git context.");
        return;
    }

    openai_agent_mode(
        state,
        model_goal,
        allow_write,
        build_after_write,
        workflow
    );
}

void openai_agent(agent_state *state, const char *goal)
{
    openai_agent_instr(
        state, goal, 0, 0, OPENAI_WORKFLOW_AGENT
    );
}

void openai_agent_plan(agent_state *state, const char *goal)
{
    openai_agent_instr(
        state, goal, 0, 0, OPENAI_WORKFLOW_PLAN
    );
}

void openai_agent_write(agent_state *state, const char *goal)
{
    openai_agent_instr(
        state, goal, 1, 0, OPENAI_WORKFLOW_WRITE
    );
}

void openai_agent_fix(agent_state *state, const char *goal)
{
    openai_agent_instr(
        state, goal, 1, 1, OPENAI_WORKFLOW_FIX
    );
}

