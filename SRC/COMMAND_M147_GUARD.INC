int command_args_empty(
    const char *arguments)
{
    if (arguments == NULL) {
        return 1;
    }

    while (*arguments == ' ' ||
           *arguments == '\t') {
        ++arguments;
    }

    return *arguments == '\0';
}
