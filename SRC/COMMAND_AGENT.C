#include "command_internal.h"
#include "openai.h"
#include "openai_execute.h"
#include "openai_plan.h"
#include "openai_repair.h"
#include "openai_state.h"
#include "openai_internal.h"

#include "command_agent_execute_dry.inc"
static void command_agent_approve(
    agent_state *state,
    const char *arguments);

void command_agent_repair_status(
    agent_state *state,
    const char *arguments);

void command_agent_repair_history(
    agent_state *state,
    const char *arguments);

void command_agent_repair_failures(
    agent_state *state,
    const char *arguments);

void command_agent_repair_show(
    agent_state *state,
    const char *arguments);

void command_agent_repair_export(
    agent_state *state,
    const char *arguments);

void command_agent_repair_stats(
    agent_state *state,
    const char *arguments);

void command_agent_repair_config(
    agent_state *state,
    const char *arguments);

void command_agent_repair_info(
    agent_state *state,
    const char *arguments);

void command_agent_repair_check(
    agent_state *state,
    const char *arguments);

void command_agent_repair_diag(
    agent_state *state,
    const char *arguments);

void command_agent_repair_count(
    agent_state *state,
    const char *arguments);

void command_agent_repair_latest(
    agent_state *state,
    const char *arguments);

void command_agent_repair_oldest(
    agent_state *state,
    const char *arguments);

void command_agent_repair_clear(
    agent_state *state,
    const char *arguments);

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
    { "AGENT/MEMORY", "Show recent stored agent history", command_agent_memory },
    { "AGENT/STATE/CLEAR", "Clear persisted workflow state with confirmation", command_agent_state_clear },
    { "AGENT/CREATE", "Create one new confirmed project file: AGENT/CREATE goal", command_agent_create },
    { "AGENT/PLAN", "Create a read-only implementation plan: AGENT/PLAN goal", command_agent_plan },
    { "AGENT/PLAN/SHOW", "Display the saved implementation plan", command_agent_plan_show },
    { "AGENT/PLAN/CLEAR", "Clear the saved implementation plan with confirmation", command_agent_plan_clear },
    { "AGENT/PLAN/VALIDATE", "Validate saved-plan file fingerprints", command_agent_plan_validate },
    { "AGENT/REPAIR/PLAN", "Create a repair plan from the last failed build", command_agent_repair_plan },
    { "AGENT/REPAIR/STATUS", "Summarize the most recent persisted repair run", command_agent_repair_status },
    { "AGENT/REPAIR/HISTORY", "Show recent persisted repair runs", command_agent_repair_history },
    { "AGENT/REPAIR/HISTORY/FAILURES", "Show failed recent persisted repair runs", command_agent_repair_failures },
    { "AGENT/REPAIR/SHOW", "Show one persisted repair run by plan hash", command_agent_repair_show },
    { "AGENT/REPAIR/EXPORT", "Export recent persisted repair history to a text file", command_agent_repair_export },
    { "AGENT/REPAIR/STATS", "Show aggregate statistics for recent repair runs", command_agent_repair_stats },
    { "AGENT/REPAIR/CONFIG", "Show resolved repair history configuration", command_agent_repair_config },
    { "AGENT/REPAIR/HISTORY/INFO", "Show persisted repair history metadata", command_agent_repair_info },
    { "AGENT/REPAIR/HISTORY/CHECK", "Check persisted repair history integrity", command_agent_repair_check },
    { "AGENT/REPAIR/HISTORY/DIAG", "Show combined repair history diagnostics", command_agent_repair_diag },
    { "AGENT/REPAIR/HISTORY/COUNT", "Show concise persisted repair counts", command_agent_repair_count },
    { "AGENT/REPAIR/HISTORY/LATEST", "Show the newest persisted repair run", command_agent_repair_latest },
    { "AGENT/REPAIR/HISTORY/OLDEST", "Show the oldest persisted repair run", command_agent_repair_oldest },
    { "AGENT/REPAIR/HISTORY/CLEAR", "Clear persisted repair history with explicit confirmation", command_agent_repair_clear },
    { "AGENT/APPROVE", "Approve the current plan for this session", command_agent_approve },
    { "AGENT/EXECUTE/DRY_RUN", "Validate and stage a saved plan without writing", command_agent_execute_dry },
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

void command_agent_repair_plan(agent_state *state,
                               const char *arguments)
{
    (void)arguments;
    openai_repair_plan(state);
}

void command_agent_repair_status(agent_state *state,
                                 const char *arguments)
{
    (void)state;
    (void)arguments;
    openai_show_repair_status();
}

void command_agent_repair_history(agent_state *state,
                                  const char *arguments)
{
    (void)state;
    (void)arguments;
    openai_show_repair_history();
}

void command_agent_repair_failures(agent_state *state,
                                   const char *arguments)
{
    (void)state;
    (void)arguments;
    openai_show_repair_failures();
}

void command_agent_repair_show(agent_state *state,
                               const char *arguments)
{
    (void)state;
    openai_show_repair_plan(arguments);
}

void command_agent_repair_export(agent_state *state,
                                 const char *arguments)
{
    (void)state;
    openai_export_repair_history(arguments);
}

void command_agent_repair_stats(agent_state *state,
                                const char *arguments)
{
    (void)state;
    (void)arguments;
    openai_show_repair_stats();
}

void command_agent_repair_config(agent_state *state,
                                 const char *arguments)
{
    (void)state;
    (void)arguments;
    openai_show_repair_config();
}

void command_agent_repair_info(agent_state *state,
                               const char *arguments)
{
    (void)state;
    (void)arguments;
    openai_show_repair_info();
}

void command_agent_repair_check(agent_state *state,
                                const char *arguments)
{
    (void)state;
    (void)arguments;
    openai_show_repair_check();
}

void command_agent_repair_diag(agent_state *state,
                               const char *arguments)
{
    (void)state;
    (void)arguments;
    openai_show_repair_diag();
}

void command_agent_repair_count(agent_state *state,
                                const char *arguments)
{
    (void)state;
    (void)arguments;
    openai_show_repair_count();
}

void command_agent_repair_latest(agent_state *state,
                                 const char *arguments)
{
    (void)state;
    (void)arguments;
    openai_show_repair_latest();
}

void command_agent_repair_oldest(agent_state *state,
                                 const char *arguments)
{
    (void)state;
    (void)arguments;
    openai_show_repair_oldest();
}

void command_agent_repair_clear(agent_state *state,
                                        const char *arguments)
{
    (void)state;
    (void)arguments;
    openai_clear_repair_cmd();
}

#include "command_agent_m147_guard.inc"

static void command_agent_approve(
    agent_state *state,
    const char *arguments)
{
    (void)state;

    if (!command_agent_approve_args(arguments)) {
        return;
    }

    openai_plan_approve();
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

void command_agent_memory(agent_state *state,
                          const char *arguments)
{
    (void)state;
    (void)arguments;
    openai_show_memory();
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

#include "command_m147_validate.inc"
