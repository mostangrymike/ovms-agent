static int openai_m146_confirm_grants(
    const char *text,
    int expected)
{
    openai_plan_session_reset();

    return
        openai_plan_confirm_text(text) == expected &&
        !openai_plan_approved &&
        openai_approved_hash == 0UL &&
        !openai_approval_invalidated;
}

int openai_m146_lower_y(void)
{
    return openai_m146_confirm_grants("y", 1);
}

int openai_m146_upper_y(void)
{
    return openai_m146_confirm_grants("Y", 1);
}

int openai_m146_yes_reject(void)
{
    return openai_m146_confirm_grants("yes", 0);
}

int openai_m146_empty_reject(void)
{
    return openai_m146_confirm_grants("", 0);
}

int openai_m146_space_reject(void)
{
    return
        openai_m146_confirm_grants(" y", 0) &&
        openai_m146_confirm_grants("y ", 0) &&
        openai_m146_confirm_grants(" ", 0);
}

int openai_m146_eof_reject(void)
{
    return openai_m146_confirm_grants(NULL, 0);
}

int openai_m146_long_reject(void)
{
    return openai_m146_confirm_grants(
        "yyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyy",
        0);
}

int openai_m146_alt_reject(void)
{
    return
        openai_m146_confirm_grants(
            "AGENT/APPROVE",
            0) &&
        openai_m146_confirm_grants(
            "AGENT/PLAN",
            0) &&
        openai_m146_confirm_grants(
            "APPROVE",
            0);
}
