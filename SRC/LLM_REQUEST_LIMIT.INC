#include "SETTINGS.H"

#define LLM_OUTPUT_TOKENS_DEFAULT 2048L
#define LLM_OUTPUT_TOKENS_MIN 64L
#define LLM_OUTPUT_TOKENS_MAX 32768L

static long llm_request_output_tokens(void)
{
    return settings_effective_long(
        "max_output_tokens",
        "OVMS_AGENT_MAX_OUTPUT_TOKENS",
        LLM_OUTPUT_TOKENS_DEFAULT,
        LLM_OUTPUT_TOKENS_MIN,
        LLM_OUTPUT_TOKENS_MAX
    );
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