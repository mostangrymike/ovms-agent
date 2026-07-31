int command_agent_approve_args(
    const char *arguments)
{
    if (!command_args_empty(arguments)) {
        (void)puts(
            "AGENT/APPROVE does not accept arguments.");
        return 0;
    }

    return 1;
}
