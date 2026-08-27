#include <stdlib.h>

#define LLM_OUTPUT_TOKENS_DEFAULT 2048L
#define LLM_OUTPUT_TOKENS_MIN 64L
#define LLM_OUTPUT_TOKENS_MAX 32768L

static long llm_request_output_tokens(void)
{
    const char *value;
    char *end;
    long parsed;

    value = getenv("OVMS_AGENT_MAX_OUTPUT_TOKENS");
    if (value == NULL || *value == '\0') {
        return LLM_OUTPUT_TOKENS_DEFAULT;
    }

    end = NULL;
    parsed = strtol(value, &end, 10);
    if (end == value || end == NULL || *end != '\0') {
        return LLM_OUTPUT_TOKENS_DEFAULT;
    }
    if (parsed < LLM_OUTPUT_TOKENS_MIN) {
        return LLM_OUTPUT_TOKENS_MIN;
    }
    if (parsed > LLM_OUTPUT_TOKENS_MAX) {
        return LLM_OUTPUT_TOKENS_MAX;
    }

    return parsed;
}

static int llm_write_output_limit(FILE *file)
{
    if (file == NULL) {
        return 0;
    }

    return fprintf(file,
                   ",\"max_output_tokens\":%ld",
                   llm_request_output_tokens()) >= 0;
}
