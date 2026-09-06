#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "llm_internal.h"
#include "LLM_AGENT_LIMITS.H"
#include "LLM_PROMPTS.H"
#include "LLM_PROJECT_MAP.H"
#include "M303_WORKFLOW_GATE.INC"

#ifndef LLM_PLAN_MAX_TURNS
#define LLM_PLAN_MAX_TURNS 24
#endif

static int llm_agent_workflow_allowed(
    agent_state *state,
    const char *command_name,
    int require_dcl)
{
    int gate;

    gate = m303_workflow_gate(
        state,
        llm_approval_name(),
        require_dcl
    );

    if (gate == M303_WORKFLOW_POLICY) {
        (void)printf(
            "%s refused: workspace approval policy required.\n",
            command_name
        );
        return 0;
    }

    if (gate == M303_WORKFLOW_WRITE) {
        (void)printf(
            "%s refused: guarded writes are disabled.\n",
            command_name
        );
        return 0;
    }

    if (gate == M303_WORKFLOW_DCL) {
        (void)printf(
            "%s refused: DCL execution is disabled.\n",
            command_name
        );
        return 0;
    }

    return 1;
}

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
    if (!llm_agent_workflow_allowed(
            state,
            "AGENT/WRITE",
            0)) {
        return;
    }

    llm_agent_instr(
        state, goal, 1, 0, LLM_WORKFLOW_WRITE
    );
}

void llm_agent_fix(agent_state *state, const char *goal)
{
    if (!llm_agent_workflow_allowed(
            state,
            "AGENT/FIX",
            1)) {
        return;
    }

    llm_agent_instr(
        state, goal, 1, 1, LLM_WORKFLOW_FIX
    );
}
