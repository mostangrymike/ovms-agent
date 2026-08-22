int openai_m126_invalid_source(void)
{
    return strcmp(
        openai_state_source_name(0, 0),
        "unavailable") == 0;
}

int openai_m126_newest_source(void)
{
    return strcmp(
        openai_state_source_name(1, 0),
        "newest version") == 0;
}

int openai_m126_recovery_source(void)
{
    return strcmp(
        openai_state_source_name(1, 1),
        "previous version recovery") == 0;
}

int openai_m126_invalid_overrides(void)
{
    return strcmp(
        openai_state_source_name(0, 1),
        "unavailable") == 0;
}
