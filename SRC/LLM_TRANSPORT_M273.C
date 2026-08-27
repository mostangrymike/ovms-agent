#include "LLM_USAGE.H"

/* M276 bounded provider-rate-limit parsing is part of the transport object. */
#include "LLM_RATE_LIMIT.C"

#define perform_openai_request m273_raw_request
#include "LLM_TRANSPORT.C"
#undef perform_openai_request

int perform_openai_request(void)
{
    int result;

    result = m273_raw_request();
    if (result) {
        (void)llm_usage_record_file(LLM_RESPONSE_FILE);
    }

    return result;
}
