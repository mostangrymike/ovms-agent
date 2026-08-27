#ifndef OVMS_AGENT_LLM_TOOL_ARGS_M275_H
#define OVMS_AGENT_LLM_TOOL_ARGS_M275_H

#include <stddef.h>
#include <string.h>

/*
 * Split AGENT/TOOL/RUN input without transcript sanitization.
 * The caller chooses the live-dispatch bound; transcript recording remains
 * independently bounded by llm_tx_append()/llm_tx_clean().
 */
static int m275_tool_args_split(const char *arguments,
                                char *name,
                                size_t name_size,
                                char *rest,
                                size_t rest_size)
{
    const char *position;
    const char *start;
    size_t length;

    if (name == NULL || name_size == 0U ||
        rest == NULL || rest_size == 0U) {
        return 0;
    }

    position = arguments != NULL ? arguments : "";

    while (*position == ' ' || *position == '\t') {
        ++position;
    }

    start = position;
    while (*position != '\0' &&
           *position != ' ' &&
           *position != '\t') {
        ++position;
    }

    length = (size_t)(position - start);
    if (length == 0U || length >= name_size) {
        return 0;
    }

    (void)memcpy(name, start, length);
    name[length] = '\0';

    while (*position == ' ' || *position == '\t') {
        ++position;
    }

    length = strlen(position);
    if (length >= rest_size) {
        return 0;
    }

    (void)memcpy(rest, position, length + 1U);
    return 1;
}

#endif
