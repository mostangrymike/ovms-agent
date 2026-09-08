static int llm_m312_final_facts(
    char *output,
    size_t output_size,
    unsigned int turns,
    unsigned int tool_calls)
{
    int written;

    if (output == NULL || output_size == 0U) {
        return 0;
    }

    written = snprintf(
        output,
        output_size,
        "Host execution facts: limit_reached=yes; turns=%u; tool_calls=%u. "
        "These host counters are authoritative; do not infer different "
        "counts from the visible conversation history.",
        turns,
        tool_calls
    );

    return written >= 0 && (size_t)written < output_size;
}
