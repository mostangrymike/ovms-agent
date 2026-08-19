#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "llm_internal.h"

#define OPENAI_RESULT_MAX 8192U
#define OPENAI_RESULT_ARG_MAX 1024U
#define OPENAI_RESULT_OUT_MAX 6144U

static char openai_result_last[OPENAI_RESULT_MAX] =
    "No normalized autonomous tool result has been captured.";

static void result_copy(char *dst, size_t dst_size,
                        const char *src, int *truncated)
{
    size_t length;
    if (dst == NULL || dst_size == 0U) return;
    if (src == NULL) src = "";
    length = strlen(src);
    if (length >= dst_size) {
        length = dst_size - 1U;
        if (truncated != NULL) *truncated = 1;
    }
    (void)memcpy(dst, src, length);
    dst[length] = '\0';
}

static int result_append(char *dst, size_t dst_size,
                         const char *text, int *truncated)
{
    size_t used;
    size_t available;
    size_t length;

    if (dst == NULL || dst_size == 0U || text == NULL) return 0;
    used = strlen(dst);
    if (used >= dst_size - 1U) {
        if (truncated != NULL) *truncated = 1;
        return 0;
    }
    available = dst_size - used - 1U;
    length = strlen(text);
    if (length > available) {
        length = available;
        if (truncated != NULL) *truncated = 1;
    }
    if (length > 0U) (void)memcpy(dst + used, text, length);
    dst[used + length] = '\0';
    return length > 0U;
}

static void result_sanitize(char *text)
{
    unsigned char *cursor;

    if (text == NULL) return;
    cursor = (unsigned char *)text;
    while (*cursor != '\0') {
        if (*cursor == '\r') *cursor = '\n';
        else if (*cursor < 32U && *cursor != '\n' && *cursor != '\t') {
            *cursor = ' ';
        }
        ++cursor;
    }
}

int openai_tool_result_normalize(const char *name,
                                 const char *arguments,
                                 const char *output,
                                 char *normalized,
                                 size_t normalized_size)
{
    char safe_name[64];
    char safe_args[OPENAI_RESULT_ARG_MAX];
    char safe_output[OPENAI_RESULT_OUT_MAX];
    int truncated;

    if (normalized == NULL || normalized_size == 0U ||
        name == NULL || *name == '\0') return 0;

    truncated = 0;
    result_copy(safe_name, sizeof(safe_name), name, &truncated);
    result_copy(safe_args, sizeof(safe_args), arguments, &truncated);
    result_copy(safe_output, sizeof(safe_output), output, &truncated);
    result_sanitize(safe_name);
    result_sanitize(safe_args);
    result_sanitize(safe_output);

    normalized[0] = '\0';
    (void)result_append(normalized, normalized_size,
                        "Tool: ", &truncated);
    (void)result_append(normalized, normalized_size,
                        safe_name, &truncated);
    (void)result_append(normalized, normalized_size,
                        "\nArguments: ", &truncated);
    (void)result_append(normalized, normalized_size,
                        safe_args[0] != '\0' ? safe_args : "(none)",
                        &truncated);
    (void)result_append(normalized, normalized_size,
                        "\nOutput:\n", &truncated);
    (void)result_append(normalized, normalized_size,
                        safe_output[0] != '\0' ? safe_output : "(none)",
                        &truncated);
    if (truncated) {
        (void)result_append(normalized, normalized_size,
                            "\n[normalized result truncated]", NULL);
    }

    result_copy(openai_result_last, sizeof(openai_result_last),
                normalized, NULL);
    return 1;
}

const char *openai_tool_result_last(void)
{
    return openai_result_last;
}
