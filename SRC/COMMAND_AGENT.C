#include "command_internal.h"
#include "openai.h"
#include "openai_execute.h"
#include "openai_plan.h"
#include "openai_state.h"

void command_agent_plan(agent_state *state,
                        const char *arguments)
{
    openai_agent_plan(state, arguments);
}

void command_agent_plan_show(agent_state *state,
                             const char *arguments)
{
    (void)state;
    (void)arguments;
    openai_plan_show();
}

void command_agent_plan_validate(agent_state *state,
                                 const char *arguments)
{
    (void)state;
    (void)arguments;
    openai_plan_validate();
}

void command_agent_plan_clear(agent_state *state,
                              const char *arguments)
{
    (void)state;
    (void)arguments;
    openai_plan_clear();
}

void command_agent_execute(agent_state *state,
                           const char *arguments)
{
    (void)arguments;
    openai_plan_execute(state);
}

void command_agent_create(agent_state *state,
                          const char *arguments)
{
    openai_agent_create(state, arguments);
}

void command_agent_state(agent_state *state,
                         const char *arguments)
{
    (void)state;
    (void)arguments;
    openai_show_state();
}

void command_agent_state_clear(agent_state *state,
                               const char *arguments)
{
    (void)state;
    (void)arguments;
    openai_clear_state();
}

void command_agent_metrics(agent_state *state,
                           const char *arguments)
{
    (void)state;
    (void)arguments;
    openai_show_metrics();
}

void command_agent_log_old(agent_state *state,
                           const char *arguments)
{
    (void)state;
    (void)arguments;
    openai_show_old_log();
}

void command_agent_log_clear(agent_state *state,
                             const char *arguments)
{
    (void)state;
    (void)arguments;
    openai_clear_log();
}

void command_agent_log(agent_state *state,
                       const char *arguments)
{
    (void)state;
    (void)arguments;
    openai_show_log();
}

void command_agent_verify(agent_state *state,
                          const char *arguments)
{
    (void)arguments;
    openai_verify(state);
}

void command_agent_status(agent_state *state,
                          const char *arguments)
{
    (void)arguments;
    openai_status(state);
}

void command_agent_selftest(agent_state *state,
                            const char *arguments)
{
    (void)arguments;
    openai_selftest(state);
}

void command_agent_retry(agent_state *state,
                         const char *arguments)
{
    openai_agent_retry(state, arguments);
}

void command_agent_fix(agent_state *state,
                       const char *arguments)
{
    openai_agent_fix(state, arguments);
}

void command_agent_build(agent_state *state,
                         const char *arguments)
{
    openai_agent_build(state, arguments);
}

void command_agent_write(agent_state *state,
                         const char *arguments)
{
    openai_agent_write(state, arguments);
}

void command_agent(agent_state *state,
                   const char *arguments)
{
    openai_agent(state, arguments);
}

void command_chat(agent_state *state,
                  const char *arguments)
{
    openai_chat(state, arguments);
}

void command_review(agent_state *state,
                    const char *arguments)
{
    openai_review_file(state, arguments);
}

void command_chat_reset(agent_state *state,
                        const char *arguments)
{
    (void)state;
    (void)arguments;
    openai_chat_reset();
}

void command_ask(agent_state *state,
                 const char *arguments)
{
    openai_ask(state, arguments);
}
