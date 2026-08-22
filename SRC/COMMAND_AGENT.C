#include "command_internal.h"
#include "LLM.H"
#include "LLM_EXECUTE.H"
#include "LLM_PLAN.H"
#include "LLM_REPAIR.H"
#include "LLM_STATE.H"
#include "llm_internal.h"

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

void command_agent_query_outcome(
    agent_state *state,
    const char *arguments);

void command_agent_query_attempts(
    agent_state *state,
    const char *arguments);

void command_agent_query_plan(
    agent_state *state,
    const char *arguments);

void command_agent_query_since(
    agent_state *state,
    const char *arguments);

void command_agent_report(
    agent_state *state,
    const char *arguments);

void command_agent_summary(
    agent_state *state,
    const char *arguments);

void command_agent_csv(
    agent_state *state,
    const char *arguments);

void command_agent_kv(
    agent_state *state,
    const char *arguments);

void command_agent_compare(
    agent_state *state,
    const char *arguments);

void command_agent_streak(
    agent_state *state,
    const char *arguments);

void command_agent_recovery(
    agent_state *state,
    const char *arguments);

void command_agent_trend(
    agent_state *state,
    const char *arguments);

void command_agent_first(
    agent_state *state,
    const char *arguments);

void command_agent_last(
    agent_state *state,
    const char *arguments);

void command_agent_range(
    agent_state *state,
    const char *arguments);

void command_agent_index(
    agent_state *state,
    const char *arguments);

void command_agent_repair_clear(
    agent_state *state,
    const char *arguments);

void command_agent_tools(
    agent_state *state, const char *arguments);
void command_agent_tool_info(
    agent_state *state, const char *arguments);
void command_agent_mcp(
    agent_state *state, const char *arguments);
void command_agent_mcp_info(
    agent_state *state, const char *arguments);
void command_agent_mcp_call(
    agent_state *state, const char *arguments);
void command_agent_mcp_execute(
    agent_state *state, const char *arguments);
void command_agent_approval(
    agent_state *state, const char *arguments);
void command_agent_approval_set(
    agent_state *state, const char *arguments);
void command_agent_approval_reset(
    agent_state *state, const char *arguments);
void command_agent_context(
    agent_state *state, const char *arguments);
void command_agent_exec_goal(
    agent_state *state, const char *arguments);
void command_agent_exec_dry(
    agent_state *state, const char *arguments);
void command_agent_parity(
    agent_state *state, const char *arguments);
void command_agent_parity_final(
    agent_state *state, const char *arguments);
void command_agent_tools_ext(
    agent_state *state, const char *arguments);
void command_agent_lang(
    agent_state *state, const char *arguments);
void command_agent_lang_info(
    agent_state *state, const char *arguments);
void command_agent_lang_detect(
    agent_state *state, const char *arguments);
void command_agent_lang_policy(
    agent_state *state, const char *arguments);
void command_agent_github(
    agent_state *state, const char *arguments);
void command_agent_gh_status(
    agent_state *state, const char *arguments);
void command_agent_gh_remote(
    agent_state *state, const char *arguments);
void command_agent_gh_check(
    agent_state *state, const char *arguments);
void command_agent_gh_fetch(
    agent_state *state, const char *arguments);
void command_agent_gh_pull(
    agent_state *state, const char *arguments);
void command_agent_gh_push(
    agent_state *state, const char *arguments);
void command_agent_gh_clone(
    agent_state *state, const char *arguments);
void command_agent_gh_issues(
    agent_state *state, const char *arguments);
void command_agent_gh_pr(
    agent_state *state, const char *arguments);

void command_agent_session_new(
    agent_state *state, const char *arguments);
void command_agent_session_list(
    agent_state *state, const char *arguments);
void command_agent_session_show(
    agent_state *state, const char *arguments);
void command_agent_session_resume(
    agent_state *state, const char *arguments);
void command_agent_session_fork(
    agent_state *state, const char *arguments);
void command_agent_session_rename(
    agent_state *state, const char *arguments);
void command_agent_session_archive(
    agent_state *state, const char *arguments);
void command_agent_session_unarc(
    agent_state *state, const char *arguments);
void command_agent_session_delete(
    agent_state *state, const char *arguments);
void command_agent_session_current(
    agent_state *state, const char *arguments);

void command_agent_session_hist(
    agent_state *state, const char *arguments);
void command_agent_sess_results(
    agent_state *state, const char *arguments);
void command_agent_sess_result_last(
    agent_state *state, const char *arguments);
void command_agent_sess_res_clear(
    agent_state *state, const char *arguments);
void command_agent_session_export2(
    agent_state *state, const char *arguments);
void command_agent_session_exec(
    agent_state *state, const char *arguments);
void command_agent_tool_run(
    agent_state *state, const char *arguments);
void command_agent_tool_last(
    agent_state *state, const char *arguments);
void command_agent_tool_hist(
    agent_state *state, const char *arguments);
void command_agent_tool_clear(
    agent_state *state, const char *arguments);
void command_agent_exec_resume(
    agent_state *state, const char *arguments);
void command_agent_exec_fork(
    agent_state *state, const char *arguments);

void command_agent_auto_status(
    agent_state *state, const char *arguments);
void command_agent_auto_limits(
    agent_state *state, const char *arguments);
void command_agent_auto_reset(
    agent_state *state, const char *arguments);

void command_agent_context_show(
    agent_state *state, const char *arguments);

void command_agent_instr_status(
    agent_state *state, const char *arguments);
void command_agent_instr_show(
    agent_state *state, const char *arguments);
void command_agent_instr_reload(
    agent_state *state, const char *arguments);

void command_agent_project_map(
    agent_state *state, const char *arguments);
void command_agent_project_files(
    agent_state *state, const char *arguments);
void command_agent_project_src(
    agent_state *state, const char *arguments);
void command_agent_project_tests(
    agent_state *state, const char *arguments);
void command_agent_project_build(
    agent_state *state, const char *arguments);
void command_agent_project_ctx(
    agent_state *state, const char *arguments);
void command_agent_project_refresh(
    agent_state *state, const char *arguments);

void command_agent_git_status2(
    agent_state *state, const char *arguments);
void command_agent_git_diff2(
    agent_state *state, const char *arguments);
void command_agent_git_changed(
    agent_state *state, const char *arguments);
void command_agent_git_context(
    agent_state *state, const char *arguments);
void command_agent_git_refresh(
    agent_state *state, const char *arguments);

void command_agent_patch(
    agent_state *state, const char *arguments);
void command_agent_patch_dry(
    agent_state *state, const char *arguments);
void command_agent_patch_validate2(
    agent_state *state, const char *arguments);
void command_agent_patch_last(
    agent_state *state, const char *arguments);

static const command_entry agent_commands[] = {
    { "CHAT", "Continue an LLM conversation: CHAT prompt", command_chat },
    { "CHAT/RESET", "Reset the LLM conversation", command_chat_reset },
    { "REVIEW", "Review source with the configured LLM: REVIEW file", command_review },
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
    { "AGENT/REPAIR/HISTORY/OUTCOME", "Filter recent repair runs by final outcome", command_agent_query_outcome },
    { "AGENT/REPAIR/HISTORY/ATTEMPTS", "Filter recent repair runs by attempt count", command_agent_query_attempts },
    { "AGENT/REPAIR/HISTORY/PLAN", "Filter recent repair runs by plan hash prefix", command_agent_query_plan },
    { "AGENT/REPAIR/HISTORY/SINCE", "Filter recent repair runs by start timestamp", command_agent_query_since },
    { "AGENT/REPAIR/HISTORY/REPORT", "Show a formatted recent repair report", command_agent_report },
    { "AGENT/REPAIR/HISTORY/SUMMARY", "Show a compact recent repair summary", command_agent_summary },
    { "AGENT/REPAIR/HISTORY/CSV", "Export recent repair history as CSV", command_agent_csv },
    { "AGENT/REPAIR/HISTORY/KV", "Export recent repair history as key/value text", command_agent_kv },
    { "AGENT/REPAIR/HISTORY/COMPARE", "Compare the newest two repair runs", command_agent_compare },
    { "AGENT/REPAIR/HISTORY/STREAK", "Show the current repair outcome streak", command_agent_streak },
    { "AGENT/REPAIR/HISTORY/RECOVERY", "Summarize two-attempt repair recovery", command_agent_recovery },
    { "AGENT/REPAIR/HISTORY/TREND", "Show recent repair success trend", command_agent_trend },
    { "AGENT/REPAIR/HISTORY/FIRST", "Show the oldest N repair runs", command_agent_first },
    { "AGENT/REPAIR/HISTORY/LAST", "Show the newest N repair runs", command_agent_last },
    { "AGENT/REPAIR/HISTORY/RANGE", "Show a newest-first repair history slice", command_agent_range },
    { "AGENT/REPAIR/HISTORY/INDEX", "Show one newest-first repair history run", command_agent_index },
    { "AGENT/REPAIR/HISTORY/CLEAR", "Clear persisted repair history with explicit confirmation", command_agent_repair_clear },
    { "AGENT/APPROVE", "Approve the current plan for this session", command_agent_approve },
    { "AGENT/EXECUTE/DRY_RUN", "Validate and stage a saved plan without writing", command_agent_execute_dry },
    { "AGENT/EXECUTE", "Execute one validated saved-plan operation", command_agent_execute },
    { "AGENT/TOOLS", "List agent tools and safety classifications", command_agent_tools },
    { "AGENT/TOOLS/EXT", "List current built-in and external parity tools", command_agent_tools_ext },
    { "AGENT/LANG", "List OpenVMS programming language support", command_agent_lang },
    { "AGENT/LANG/INFO", "Show guidance for one OpenVMS programming language", command_agent_lang_info },
    { "AGENT/LANG/DETECT", "Detect language from an OpenVMS source file name", command_agent_lang_detect },
    { "AGENT/LANG/POLICY", "Show multilingual policy injected into agent context", command_agent_lang_policy },
    { "AGENT/GITHUB", "Show guarded GitHub integration and policy", command_agent_github },
    { "AGENT/GITHUB/STATUS", "Show local Git working-tree status", command_agent_gh_status },
    { "AGENT/GITHUB/REMOTE", "Show configured Git remotes", command_agent_gh_remote },
    { "AGENT/GITHUB/CHECK", "Check OpenVMS Git network prerequisites", command_agent_gh_check },
    { "AGENT/GITHUB/FETCH", "Fetch from a GitHub remote", command_agent_gh_fetch },
    { "AGENT/GITHUB/PULL", "Pull from a GitHub remote and branch", command_agent_gh_pull },
    { "AGENT/GITHUB/PUSH", "Push to a GitHub remote and branch", command_agent_gh_push },
    { "AGENT/GITHUB/CLONE", "Clone a validated GitHub repository", command_agent_gh_clone },
    { "AGENT/GITHUB/ISSUES", "Use configured GitHub bridge for issue operations", command_agent_gh_issues },
    { "AGENT/GITHUB/PR", "Use configured GitHub bridge for pull-request operations", command_agent_gh_pr },
    { "AGENT/TOOLS/INFO", "Show metadata for one agent tool", command_agent_tool_info },
    { "AGENT/MCP", "List configured MCP tool servers", command_agent_mcp },
    { "AGENT/MCP/INFO", "Show one configured MCP tool server", command_agent_mcp_info },
    { "AGENT/MCP/CALL", "Validate and plan one guarded MCP tool call", command_agent_mcp_call },
    { "AGENT/MCP/EXEC", "Execute and record one approved MCP tool call", command_agent_mcp_execute },
    { "AGENT/APPROVAL", "Show the current agent approval policy", command_agent_approval },
    { "AGENT/APPROVAL/SET", "Set the session approval policy", command_agent_approval_set },
    { "AGENT/APPROVAL/RESET", "Reset the session approval policy", command_agent_approval_reset },
    { "AGENT/CONTEXT", "Show the current agent execution context", command_agent_context },
    { "AGENT/EXEC", "Run a goal using the current approval policy", command_agent_exec_goal },
    { "AGENT/EXEC/DRY", "Create a read-only implementation plan for a goal", command_agent_exec_dry },
    { "AGENT/PARITY", "Show legacy Codex-parity capability status", command_agent_parity },
    { "AGENT/PARITY/FINAL", "Show final practical Codex parity report", command_agent_parity_final },
    { "AGENT/SESSION/NEW", "Create and select a persistent session", command_agent_session_new },
    { "AGENT/SESSION/LIST", "List persistent agent sessions", command_agent_session_list },
    { "AGENT/SESSION/SHOW", "Show one persistent agent session", command_agent_session_show },
    { "AGENT/SESSION/RESUME", "Resume an active persistent session", command_agent_session_resume },
    { "AGENT/SESSION/FORK", "Fork a persistent session", command_agent_session_fork },
    { "AGENT/SESSION/RENAME", "Rename a persistent session", command_agent_session_rename },
    { "AGENT/SESSION/ARCHIVE", "Archive a persistent session", command_agent_session_archive },
    { "AGENT/SESSION/UNARCHIVE", "Unarchive a persistent session", command_agent_session_unarc },
    { "AGENT/SESSION/DELETE", "Delete a persistent session", command_agent_session_delete },
    { "AGENT/SESSION/CURRENT", "Show the current persistent session", command_agent_session_current },
    { "AGENT/SESSION/HISTORY", "Show the persistent transcript for a session", command_agent_session_hist },
    { "AGENT/SESSION/RESULTS", "Show normalized tool evidence for a session", command_agent_sess_results },
    { "AGENT/SESSION/RESULTS/LAST", "Show the latest normalized tool result for a session", command_agent_sess_result_last },
    { "AGENT/SESSION/RESULTS/CLEAR", "Clear normalized tool evidence for a session", command_agent_sess_res_clear },
    { "AGENT/SESSION/EXPORT", "Export one persistent session transcript", command_agent_session_export2 },
    { "AGENT/SESSION/EXEC", "Resume a session and execute a goal", command_agent_session_exec },
    { "AGENT/TOOL/RUN", "Run one approved built-in tool directly", command_agent_tool_run },
    { "AGENT/TOOL/LAST", "Show the last persisted tool invocation", command_agent_tool_last },
    { "AGENT/TOOL/HISTORY", "Show recent persisted tool invocations", command_agent_tool_hist },
    { "AGENT/TOOL/CLEAR", "Clear persisted tool/session transcript history", command_agent_tool_clear },
    { "AGENT/EXEC/RESUME", "Execute a goal in the current session", command_agent_exec_resume },
    { "AGENT/EXEC/FORK", "Fork the current session and execute a goal", command_agent_exec_fork },
    { "AGENT/AUTO/STATUS", "Show last autonomous loop counters and stop reason", command_agent_auto_status },
    { "AGENT/AUTO/LIMITS", "Show autonomous model-turn and write limits", command_agent_auto_limits },
    { "AGENT/AUTO/RESET", "Reset autonomous loop status counters", command_agent_auto_reset },
    { "AGENT/CONTEXT/SHOW", "Show restored context for the current persistent session", command_agent_context_show },
    { "AGENT/INSTRUCTIONS", "Show project instruction discovery status", command_agent_instr_status },
    { "AGENT/INSTRUCTIONS/STATUS", "Show project instruction discovery status", command_agent_instr_status },
    { "AGENT/INSTRUCTIONS/SHOW", "Show loaded project instructions", command_agent_instr_show },
    { "AGENT/INSTRUCTIONS/RELOAD", "Reload project instructions from disk", command_agent_instr_reload },
    { "AGENT/PROJECT/MAP", "Show bounded repository map", command_agent_project_map },
    { "AGENT/PROJECT/FILES", "Show bounded repository file inventory", command_agent_project_files },
    { "AGENT/PROJECT/SOURCES", "Show likely source files", command_agent_project_src },
    { "AGENT/PROJECT/TESTS", "Show likely regression/test files", command_agent_project_tests },
    { "AGENT/PROJECT/BUILD", "Show likely build/control files", command_agent_project_build },
    { "AGENT/PROJECT/CONTEXT", "Show repository context injected into agent runs", command_agent_project_ctx },
    { "AGENT/PROJECT/REFRESH", "Refresh cached repository map", command_agent_project_refresh },
    { "AGENT/GIT/STATUS", "Show cached bounded Git working-tree status", command_agent_git_status2 },
    { "AGENT/GIT/DIFF", "Show cached bounded unstaged Git diff", command_agent_git_diff2 },
    { "AGENT/GIT/CHANGED", "Show changed paths from Git status", command_agent_git_changed },
    { "AGENT/GIT/CONTEXT", "Show Git context injected into agent runs", command_agent_git_context },
    { "AGENT/GIT/REFRESH", "Refresh cached Git status and diff", command_agent_git_refresh },
    { "AGENT/PATCH", "Apply one prevalidated structured multi-hunk patch file", command_agent_patch },
    { "AGENT/PATCH/DRY", "Validate a structured patch without writing", command_agent_patch_dry },
    { "AGENT/PATCH/VALIDATE", "Validate every structured patch hunk without writing", command_agent_patch_validate2 },
    { "AGENT/PATCH/LAST", "Show the most recent structured patch result", command_agent_patch_last },
    { "ASK", "Send a prompt to the configured LLM: ASK prompt", command_ask }
};

void command_register_agent(void)
{
    (void)command_registry_add(
        agent_commands,
        sizeof(agent_commands) / sizeof(agent_commands[0])
    );
}

void command_agent_patch(agent_state *state, const char *arguments)
{
    (void)state; openai_patch_apply_cmd(arguments);
}
void command_agent_patch_dry(agent_state *state, const char *arguments)
{
    (void)state; openai_patch_dry_cmd(arguments);
}
void command_agent_patch_validate2(agent_state *state, const char *arguments)
{
    (void)state; openai_patch_validate_cmd(arguments);
}
void command_agent_patch_last(agent_state *state, const char *arguments)
{
    (void)state; (void)arguments; openai_patch_last_cmd();
}

void command_agent_plan(agent_state *state,
                        const char *arguments)
{
    llm_agent_plan(state, arguments);
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
    llm_repair_plan(state);
}

void command_agent_tools(agent_state *state, const char *arguments)
{
    (void)state; (void)arguments; openai_show_tools();
}

void command_agent_tool_info(agent_state *state, const char *arguments)
{
    (void)state; openai_show_tool_info(arguments);
}

void command_agent_mcp(agent_state *state, const char *arguments)
{
    (void)state; (void)arguments; openai_show_mcp();
}

void command_agent_mcp_info(agent_state *state, const char *arguments)
{
    (void)state; openai_show_mcp_info(arguments);
}

void command_agent_mcp_call(agent_state *state, const char *arguments)
{
    (void)state; openai_show_mcp_call(arguments);
}

void command_agent_mcp_execute(agent_state *state, const char *arguments)
{
    (void)state; openai_show_mcp_execute(arguments);
}

void command_agent_approval(agent_state *state, const char *arguments)
{
    (void)state; (void)arguments; openai_show_approval();
}

void command_agent_approval_set(agent_state *state, const char *arguments)
{
    (void)state; openai_set_approval_cmd(arguments);
}

void command_agent_approval_reset(agent_state *state, const char *arguments)
{
    (void)state; (void)arguments; openai_reset_approval();
}

void command_agent_context(agent_state *state, const char *arguments)
{
    (void)arguments; openai_show_context(state);
}

void command_agent_exec_goal(agent_state *state, const char *arguments)
{
    openai_exec_goal(state, arguments);
}

void command_agent_exec_dry(agent_state *state, const char *arguments)
{
    openai_exec_dry(state, arguments);
}

void command_agent_parity(agent_state *state, const char *arguments)
{
    (void)state; (void)arguments; openai_show_parity();
}

void command_agent_parity_final(agent_state *state, const char *arguments)
{
    (void)state; (void)arguments; openai_show_final_parity();
}

void command_agent_tools_ext(agent_state *state, const char *arguments)
{
    (void)state; (void)arguments; openai_show_tools_ext();
}

void command_agent_lang(agent_state *state, const char *arguments)
{
    (void)state; (void)arguments; openai_show_langs();
}

void command_agent_lang_info(agent_state *state, const char *arguments)
{
    (void)state; openai_show_lang_info(arguments);
}

void command_agent_lang_detect(agent_state *state, const char *arguments)
{
    (void)state; openai_show_lang_detect(arguments);
}

void command_agent_lang_policy(agent_state *state, const char *arguments)
{
    (void)state; (void)arguments; openai_show_lang_policy();
}

void command_agent_github(agent_state *state, const char *arguments)
{
    (void)state; openai_show_github("help", arguments);
}

void command_agent_gh_status(agent_state *state, const char *arguments)
{
    (void)state; openai_show_github("status", arguments);
}

void command_agent_gh_remote(agent_state *state, const char *arguments)
{
    (void)state; openai_show_github("remote", arguments);
}

void command_agent_gh_check(agent_state *state, const char *arguments)
{
    (void)state; openai_show_github("check", arguments);
}

void command_agent_gh_fetch(agent_state *state, const char *arguments)
{
    (void)state; openai_show_github("fetch", arguments);
}

void command_agent_gh_pull(agent_state *state, const char *arguments)
{
    (void)state; openai_show_github("pull", arguments);
}

void command_agent_gh_push(agent_state *state, const char *arguments)
{
    (void)state; openai_show_github("push", arguments);
}

void command_agent_gh_clone(agent_state *state, const char *arguments)
{
    (void)state; openai_show_github("clone", arguments);
}

void command_agent_gh_issues(agent_state *state, const char *arguments)
{
    (void)state; openai_show_github("issues", arguments);
}

void command_agent_gh_pr(agent_state *state, const char *arguments)
{
    (void)state; openai_show_github("pr", arguments);
}

void command_agent_session_new(agent_state *state, const char *arguments)
{
    (void)state; openai_session_new_cmd(arguments);
}

void command_agent_session_list(agent_state *state, const char *arguments)
{
    (void)state; (void)arguments; openai_show_session_list();
}

void command_agent_session_show(agent_state *state, const char *arguments)
{
    (void)state; openai_show_session(arguments);
}

void command_agent_session_resume(agent_state *state, const char *arguments)
{
    (void)state; openai_session_resume_cmd(arguments);
}

void command_agent_session_fork(agent_state *state, const char *arguments)
{
    (void)state; openai_session_fork_cmd(arguments);
}

void command_agent_session_rename(agent_state *state, const char *arguments)
{
    (void)state; openai_session_rename_cmd(arguments);
}

void command_agent_session_archive(agent_state *state, const char *arguments)
{
    (void)state; openai_session_archive_cmd(arguments);
}

void command_agent_session_unarc(agent_state *state, const char *arguments)
{
    (void)state; openai_session_unarc_cmd(arguments);
}

void command_agent_session_delete(agent_state *state, const char *arguments)
{
    (void)state; openai_session_delete_cmd(arguments);
}

void command_agent_session_current(agent_state *state, const char *arguments)
{
    (void)state; (void)arguments; openai_show_session_current();
}

void command_agent_session_hist(agent_state *state, const char *arguments)
{
    (void)state; openai_show_session_hist(arguments);
}

void command_agent_sess_results(agent_state *state, const char *arguments)
{
    (void)state; openai_show_session_results(arguments);
}

void command_agent_sess_result_last(agent_state *state, const char *arguments)
{
    (void)state; openai_show_session_result(arguments);
}

void command_agent_sess_res_clear(agent_state *state, const char *arguments)
{
    (void)state; openai_session_clear_res_cmd(arguments);
}

void command_agent_session_export2(agent_state *state, const char *arguments)
{
    (void)state; openai_session_export_cmd(arguments);
}

void command_agent_session_exec(agent_state *state, const char *arguments)
{
    openai_session_exec_cmd(state, arguments);
}

void command_agent_tool_run(agent_state *state, const char *arguments)
{
    openai_tool_run_cmd(state, arguments);
}

void command_agent_tool_last(agent_state *state, const char *arguments)
{
    (void)state; (void)arguments; openai_show_tool_last();
}

void command_agent_tool_hist(agent_state *state, const char *arguments)
{
    (void)state; (void)arguments; openai_show_tool_hist();
}

void command_agent_tool_clear(agent_state *state, const char *arguments)
{
    (void)state; openai_tool_clear_cmd(arguments);
}

void command_agent_exec_resume(agent_state *state, const char *arguments)
{
    openai_exec_resume_cmd(state, arguments);
}

void command_agent_exec_fork(agent_state *state, const char *arguments)
{
    openai_exec_fork_cmd(state, arguments);
}

void command_agent_auto_status(agent_state *state, const char *arguments)
{
    (void)state;
    (void)arguments;
    openai_show_auto_status();
}

void command_agent_auto_limits(agent_state *state, const char *arguments)
{
    (void)state;
    (void)arguments;
    openai_show_auto_limits();
}

void command_agent_auto_reset(agent_state *state, const char *arguments)
{
    (void)state;
    (void)arguments;
    openai_auto_reset();
    (void)puts("Autonomous loop status reset.");
}

void command_agent_context_show(agent_state *state, const char *arguments)
{
    (void)state;
    (void)arguments;
    llm_show_context_current();
}

void command_agent_instr_status(agent_state *state, const char *arguments)
{
    (void)arguments;
    llm_show_instr_status(state);
}

void command_agent_instr_show(agent_state *state, const char *arguments)
{
    (void)arguments;
    llm_show_instr(state);
}

void command_agent_instr_reload(agent_state *state, const char *arguments)
{
    (void)arguments;
    llm_instr_reload_cmd(state);
}

void command_agent_project_map(agent_state *state, const char *arguments)
{
    (void)arguments;
    llm_show_project_map(state, 0);
}

void command_agent_project_files(agent_state *state, const char *arguments)
{
    (void)arguments;
    llm_show_project_map(state, 0);
}

void command_agent_project_src(agent_state *state, const char *arguments)
{
    (void)arguments;
    llm_show_project_map(state, 1);
}

void command_agent_project_tests(agent_state *state, const char *arguments)
{
    (void)arguments;
    llm_show_project_map(state, 3);
}

void command_agent_project_build(agent_state *state, const char *arguments)
{
    (void)arguments;
    llm_show_project_map(state, 4);
}

void command_agent_project_ctx(agent_state *state, const char *arguments)
{
    (void)arguments;
    llm_show_project_ctx(state);
}

void command_agent_project_refresh(agent_state *state, const char *arguments)
{
    (void)arguments;
    llm_project_refresh_cmd(state);
}

void command_agent_git_status2(agent_state *state, const char *arguments)
{
    (void)arguments;
    openai_show_git_status(state);
}

void command_agent_git_diff2(agent_state *state, const char *arguments)
{
    (void)arguments;
    openai_show_git_diff(state);
}

void command_agent_git_changed(agent_state *state, const char *arguments)
{
    (void)arguments;
    openai_show_git_changed(state);
}

void command_agent_git_context(agent_state *state, const char *arguments)
{
    (void)arguments;
    openai_show_git_context(state);
}

void command_agent_git_refresh(agent_state *state, const char *arguments)
{
    (void)arguments;
    openai_git_refresh_cmd(state);
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

void command_agent_query_outcome(agent_state *state,
                                 const char *arguments)
{
    (void)state;
    openai_show_query_outcome(arguments);
}

void command_agent_query_attempts(agent_state *state,
                                  const char *arguments)
{
    (void)state;
    openai_show_query_attempts(arguments);
}

void command_agent_query_plan(agent_state *state,
                              const char *arguments)
{
    (void)state;
    openai_show_query_plan(arguments);
}

void command_agent_query_since(agent_state *state,
                               const char *arguments)
{
    (void)state;
    openai_show_query_since(arguments);
}

void command_agent_report(agent_state *state,
                          const char *arguments)
{
    (void)state;
    (void)arguments;
    openai_show_report();
}

void command_agent_summary(agent_state *state,
                           const char *arguments)
{
    (void)state;
    (void)arguments;
    openai_show_summary();
}

void command_agent_csv(agent_state *state,
                       const char *arguments)
{
    (void)state;
    openai_export_csv(arguments);
}

void command_agent_kv(agent_state *state,
                      const char *arguments)
{
    (void)state;
    openai_export_kv(arguments);
}

void command_agent_compare(agent_state *state,
                           const char *arguments)
{
    (void)state;
    (void)arguments;
    openai_show_compare();
}

void command_agent_streak(agent_state *state,
                          const char *arguments)
{
    (void)state;
    (void)arguments;
    openai_show_streak();
}

void command_agent_recovery(agent_state *state,
                            const char *arguments)
{
    (void)state;
    (void)arguments;
    openai_show_recovery();
}

void command_agent_trend(agent_state *state,
                         const char *arguments)
{
    (void)state;
    (void)arguments;
    openai_show_trend();
}

void command_agent_first(agent_state *state,
                         const char *arguments)
{
    (void)state;
    openai_show_first(arguments);
}

void command_agent_last(agent_state *state,
                        const char *arguments)
{
    (void)state;
    openai_show_last(arguments);
}

void command_agent_range(agent_state *state,
                         const char *arguments)
{
    (void)state;
    openai_show_range(arguments);
}

void command_agent_index(agent_state *state,
                         const char *arguments)
{
    (void)state;
    openai_show_index(arguments);
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
    llm_agent_create(state, arguments);
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
    llm_agent_retry(state, arguments);
}

void command_agent_fix(agent_state *state,
                       const char *arguments)
{
    llm_agent_fix(state, arguments);
}

void command_agent_build(agent_state *state,
                         const char *arguments)
{
    llm_agent_build(state, arguments);
}

void command_agent_write(agent_state *state,
                         const char *arguments)
{
    llm_agent_write(state, arguments);
}

void command_agent(agent_state *state,
                   const char *arguments)
{
    llm_agent(state, arguments);
}

void command_chat(agent_state *state,
                  const char *arguments)
{
    llm_chat(state, arguments);
}

void command_review(agent_state *state,
                    const char *arguments)
{
    llm_review_file(state, arguments);
}

void command_chat_reset(agent_state *state,
                        const char *arguments)
{
    (void)state;
    (void)arguments;
    llm_chat_reset();
}

void command_ask(agent_state *state,
                 const char *arguments)
{
    llm_ask(state, arguments);
}

#include "command_m147_validate.inc"
