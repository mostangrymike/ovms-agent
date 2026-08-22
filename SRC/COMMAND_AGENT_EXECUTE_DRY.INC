void command_agent_execute_dry(
    agent_state *state,
    const char *arguments)
{
    (void)arguments;
    llm_plan_execute_dry(state);
}
