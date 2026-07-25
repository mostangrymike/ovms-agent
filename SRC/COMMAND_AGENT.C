#include "command_internal.h"
#include "openai.h"
#include "openai_execute.h"
#include "openai_plan.h"
#include "openai_state.h"

static const command_entry agent_commands[] = {
    { "CHAT", "Continue an OpenAI conversation: CHAT prompt", command_chat },
    { "CHAT/RESET", "Reset the OpenAI conversation", command_chat_reset },
    { "REVIEW", "Review source with OpenAI: REVIEW file", command_review },
    { "AGENT", "Run read-only AI agent: AGENT goal", command_agent },
    { "AGENT/WRITE", "Run guarded write agent: AGENT/WRITE goal", command_agent_write },
    { "AGENT/BUILD", "Run controlled project build analysis: AGENT/BUILD goal", command_agent_build },
    { "AGENT/FIX", "Apply one confirmed fix and build: AGENT/FIX goal", command_agent_fix },
    { "AGENT/RETRY", "Retry one supervised fix using prior build diagnostics: AGENT/RETRY goal", command_agent_retry },
    { "AGENT/SELFTEST", "Run non-destructive agent checks", command_agent_selftest },
    { "AGENT/STATUS", "Show agent capabilities and recent workflow state", command_agent_status },
    { "AGENT/VERIFY", "Run safety checks and a controlled build", command_agent_verify },
    { "AGENT/LOG", "Display the structured local activity log", command_agent_log },
    { "AGENT/LOG/OLD", "Display the rotated activity log", command_agent_log_old },
    { "AGENT/LOG/CLEAR", "Clear the active activity log with confirmation", command_agent_log_clear },
    { "AGENT/METRICS", "Summarize structured activity metrics", command_agent_metrics },
    { "AGENT/STATE", "Display validated persistent workflow state", command_agent_state },
    { "AGENT/STATE/CLEAR", "Clear persisted workflow state with confirmation", command_agent_state_clear },
    { "AGENT/CREATE", "Create one new confirmed project file: AGENT/CREATE goal", command_agent_create },
    { "AGENT/PLAN", "Create a read-only implementation plan: AGENT/PLAN goal", command_agent_plan },
    { "AGENT/PLAN/SHOW", "Display the saved implementation plan", command_agent_plan_show },
    { "AGENT/PLAN/CLEAR", "Clear the saved implementation plan with confirmation", command_agent_plan_clear },
    { "AGENT/PLAN/VALIDATE", "Validate saved-plan file fingerprints", command_agent_plan_validate },
    { "AGENT/EXECUTE", "Execute one validated saved-plan operation", command_agent_execute },
    { "ASK", "Send a prompt to OpenAI: ASK prompt", command_ask }
};

void command_register_agent(void)
{
    (void)command_registry_add(
        agent_commands,
        sizeof(agent_commands) / sizeof(agent_commands[0])
    );
}

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
