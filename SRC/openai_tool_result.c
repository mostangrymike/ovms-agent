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

char *openai_result_make(const char *tool,
                         const char *status,
                         const char *effect,
                         int code,
                         const char *arguments,
                         const char *output)
{
    char arg_text[OPENAI_RESULT_ARG_MAX];
    char out_text[OPENAI_RESULT_OUT_MAX];
    char *result;
    int truncated = 0;
    int written;

    result_copy(arg_text, sizeof(arg_text), arguments, &truncated);
    result_copy(out_text, sizeof(out_text), output, &truncated);

    result = (char *)malloc(OPENAI_RESULT_MAX);
    if (result == NULL) return NULL;

    written = snprintf(
        result, OPENAI_RESULT_MAX,
        "TOOL RESULT\n"
        "-----------\n"
        "tool: %s\n"
        "status: %s\n"
        "code: %d\n"
        "effect: %s\n"
        "truncated: %s\n"
        "arguments:\n%s\n"
        "output:\n%s\n",
        tool != NULL ? tool : "unknown",
        status != NULL ? status : "unknown",
        code,
        effect != NULL ? effect : "unknown",
        truncated ? "yes" : "no",
        arg_text[0] != '\0' ? arg_text : "(none)",
        out_text[0] != '\0' ? out_text : "(none)"
    );

    if (written < 0 || (size_t)written >= OPENAI_RESULT_MAX) {
        free(result);
        return NULL;
    }

    (void)strncpy(openai_result_last, result,
                  sizeof(openai_result_last) - 1U);
    openai_result_last[sizeof(openai_result_last) - 1U] = '\0';
    return result;
}

char *openai_build_result(const char *output, int status)
{
    return openai_result_make(
        "run_build",
        (status & 1) != 0 ? "success" : "failure",
        "execute",
        status,
        "{}",
        output != NULL ? output : "Unable to capture build output."
    );
}

int openai_result_last_text(char *output, size_t output_size)
{
    int written;
    if (output == NULL || output_size == 0U) return 0;
    written = snprintf(output, output_size, "%s", openai_result_last);
    return written >= 0 && (size_t)written < output_size;
}

void openai_show_result_last(void)
{
    (void)puts(openai_result_last);
}
