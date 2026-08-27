#include <stdio.h>
#include <stdlib.h>

#include "LLM_RATE_LIMIT.H"

int main(void)
{
    static const char groq_error[] =
        "{\"error\":{\"message\":\"Rate limit reached for model. "
        "Limit 8000, Used 5020, Requested 4361. "
        "Please try again in 10.3575s.\","
        "\"type\":\"tokens\",\"code\":\"rate_limit_exceeded\"}}";
    static const char short_error[] =
        "{\"error\":{\"message\":\"Rate limit reached. "
        "Please try again in 2.595s.\","
        "\"code\":\"rate_limit_exceeded\"}}";
    static const char auth_error[] =
        "{\"error\":{\"message\":\"Invalid API key\","
        "\"code\":\"invalid_api_key\"}}";
    static const char no_delay[] =
        "{\"error\":{\"message\":\"Rate limit reached.\","
        "\"code\":\"rate_limit_exceeded\"}}";
    static const char long_delay[] =
        "{\"error\":{\"message\":\"Rate limit reached. "
        "Please try again in 60.1s.\","
        "\"code\":\"rate_limit_exceeded\"}}";
    static const char normal_output[] =
        "{\"output\":[{\"content\":[{\"text\":"
        "\"Please try again in 10.3s if a rate limit occurs.\"}]}]}";
    unsigned int wait_seconds;

    wait_seconds = 0U;
    if (!llm_rate_limit_delay(groq_error, &wait_seconds) ||
        wait_seconds != 11U) {
        (void)puts("M276 failed: Groq decimal rate-limit delay.");
        return EXIT_FAILURE;
    }

    wait_seconds = 0U;
    if (!llm_rate_limit_delay(short_error, &wait_seconds) ||
        wait_seconds != 3U) {
        (void)puts("M276 failed: short rate-limit delay.");
        return EXIT_FAILURE;
    }

    if (llm_rate_limit_delay(auth_error, &wait_seconds) ||
        llm_rate_limit_delay(no_delay, &wait_seconds) ||
        llm_rate_limit_delay(long_delay, &wait_seconds) ||
        llm_rate_limit_delay(normal_output, &wait_seconds)) {
        (void)puts("M276 failed: non-retryable response accepted.");
        return EXIT_FAILURE;
    }

    (void)puts("M276 provider rate-limit parser regression passed.");
    return EXIT_SUCCESS;
}
