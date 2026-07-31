int openai_m147_exact_dispatch(void)
{
    return command_find("AGENT/APPROVE") != NULL;
}

int openai_m147_lower_dispatch(void)
{
    char command[] = "agent/approve";
    char *cursor;

    cursor = command;

    while (*cursor != '\0') {
        if (*cursor >= 'a' &&
            *cursor <= 'z') {
            *cursor = (char)(
                *cursor - 'a' + 'A');
        }

        ++cursor;
    }

    return command_find(command) != NULL;
}

int openai_m147_abbrev_reject(void)
{
    return command_find("AGENT/APP") == NULL;
}

int openai_m147_suffix_reject(void)
{
    return
        command_find("AGENT/APPROVE/") == NULL &&
        command_find("AGENT/APPROVE/X") == NULL;
}

int openai_m147_argument_reject(void)
{
    return
        !command_agent_approve_args("NOW") &&
        !command_agent_approve_args("Y");
}

int openai_m147_duplicate_reject(void)
{
    return !command_agent_approve_args(
        "AGENT/APPROVE");
}

int openai_m147_space_accept(void)
{
    return
        command_agent_approve_args(NULL) &&
        command_agent_approve_args("") &&
        command_agent_approve_args("   ") &&
        command_agent_approve_args("\t\t");
}

int openai_m147_reject_unchanged(void)
{
    int approved_before;
    unsigned long hash_before;
    int invalid_before;

    openai_plan_approval_clear();

    approved_before = openai_plan_approved;
    hash_before = openai_approved_hash;
    invalid_before = openai_approval_invalidated;

    return
        !command_agent_approve_args("NOW") &&
        openai_plan_approved == approved_before &&
        openai_approved_hash == hash_before &&
        openai_approval_invalidated == invalid_before;
}
