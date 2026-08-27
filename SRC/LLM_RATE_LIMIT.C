#include <stdlib.h>
#include <string.h>

#include "LLM_RATE_LIMIT.H"

#define LLM_RATE_WAIT_MAX 60U

int llm_rate_limit_delay(const char *json,
                         unsigned int *wait_seconds)
{
    static const char retry_marker[] = "Please try again in ";
    const char *position;
    char *end;
    double delay;
    unsigned int seconds;

    if (json == NULL || wait_seconds == NULL) {
        return 0;
    }

    *wait_seconds = 0U;

    if (strstr(json, "\"error\"") == NULL ||
        (strstr(json, "rate_limit_exceeded") == NULL &&
         strstr(json, "Rate limit reached") == NULL)) {
        return 0;
    }

    position = strstr(json, retry_marker);
    if (position == NULL) {
        return 0;
    }

    position += strlen(retry_marker);
    end = NULL;
    delay = strtod(position, &end);

    if (end == position || delay <= 0.0 ||
        (*end != 's' && *end != 'S')) {
        return 0;
    }

    seconds = (unsigned int)delay;
    if ((double)seconds < delay) {
        ++seconds;
    }

    if (seconds == 0U || seconds > LLM_RATE_WAIT_MAX) {
        return 0;
    }

    *wait_seconds = seconds;
    return 1;
}
