int llm_m147_exact_dispatch(void)
{
    return command_find("AGENT/APPROVE") != NULL;
}

int llm_m147_lower_dispatch(void)
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

int llm_m147_abbrev_reject(void)
{
    return command_find("AGENT/APP") == NULL;
}

int llm_m147_suffix_reject(void)
{
    return
        command_find("AGENT/APPROVE/") == NULL &&
        command_find("AGENT/APPROVE/X") == NULL;
}

int llm_m147_argument_reject(void)
{
    return
        !command_agent_approve_args("NOW") &&
        !command_agent_approve_args("Y");
}

int llm_m147_duplicate_reject(void)
{
    return !command_agent_approve_args(
        "AGENT/APPROVE");
}

int llm_m147_space_accept(void)
{
    return
        command_agent_approve_args(NULL) &&
        command_agent_approve_args("") &&
        command_agent_approve_args("   ") &&
        command_agent_approve_args("\t\t");
}

int llm_m147_reject_unchanged(void)
{
    int approved_before;
    unsigned long hash_before;
    int invalid_before;

    llm_plan_approval_clear();

    approved_before = llm_plan_approved;
    hash_before = llm_approved_hash;
    invalid_before = llm_approval_invalidated;

    return
        !command_agent_approve_args("NOW") &&
        llm_plan_approved == approved_before &&
        llm_approved_hash == hash_before &&
        llm_approval_invalidated == invalid_before;
}
