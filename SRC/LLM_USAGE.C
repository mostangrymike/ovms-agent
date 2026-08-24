#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "LLM_JSON_PARSE.H"
#include "LLM_USAGE.H"

static llm_usage_stats m273_usage;

static unsigned long m273_usage_add(unsigned long left,
                                     unsigned long right)
{
    if (ULONG_MAX - left < right) {
        return ULONG_MAX;
    }

    return left + right;
}

static const char *m273_top_key(const char *json,
                                const char *key)
{
    const char *position;
    int depth;
    int in_string;
    int escaped;

    if (json == NULL || key == NULL || *key == '\0') {
        return NULL;
    }

    depth = 0;
    in_string = 0;
    escaped = 0;

    for (position = json; *position != '\0'; ++position) {
        char ch;

        ch = *position;

        if (in_string) {
            if (escaped) {
                escaped = 0;
            } else if (ch == '\\') {
                escaped = 1;
            } else if (ch == '"') {
                in_string = 0;
            }
            continue;
        }

        if (ch == '"') {
            if (depth == 1) {
                const char *value;

                value = NULL;
                if (json_key_matches(position, key, &value)) {
                    return value;
                }
            }
            in_string = 1;
        } else if (ch == '{') {
            ++depth;
        } else if (ch == '}') {
            if (depth > 0) {
                --depth;
            }
        }
    }

    return NULL;
}

static char *m273_copy_object(const char *start)
{
    const char *position;
    int depth;
    int in_string;
    int escaped;
    size_t length;
    char *result;

    if (start == NULL || *start != '{') {
        return NULL;
    }

    depth = 0;
    in_string = 0;
    escaped = 0;

    for (position = start; *position != '\0'; ++position) {
        char ch;

        ch = *position;

        if (in_string) {
            if (escaped) {
                escaped = 0;
            } else if (ch == '\\') {
                escaped = 1;
            } else if (ch == '"') {
                in_string = 0;
            }
        } else if (ch == '"') {
            in_string = 1;
        } else if (ch == '{') {
            ++depth;
        } else if (ch == '}') {
            --depth;
            if (depth == 0) {
                ++position;
                length = (size_t)(position - start);
                result = (char *)malloc(length + 1U);
                if (result == NULL) {
                    return NULL;
                }
                (void)memcpy(result, start, length);
                result[length] = '\0';
                return result;
            }
        }
    }

    return NULL;
}

int llm_usage_record_json(const char *json)
{
    const char *usage_start;
    char *usage;
    long input_tokens;
    long output_tokens;
    long total_tokens;

    m273_usage.last_known = 0;

    usage_start = m273_top_key(json, "usage");
    if (usage_start == NULL || *usage_start != '{') {
        return 0;
    }

    usage = m273_copy_object(usage_start);
    if (usage == NULL) {
        return 0;
    }

    input_tokens = 0L;
    output_tokens = 0L;
    total_tokens = 0L;

    if (!extract_integer_argument(usage,
                                  "input_tokens",
                                  &input_tokens) ||
        !extract_integer_argument(usage,
                                  "output_tokens",
                                  &output_tokens) ||
        !extract_integer_argument(usage,
                                  "total_tokens",
                                  &total_tokens) ||
        input_tokens < 0L ||
        output_tokens < 0L ||
        total_tokens < 0L) {
        free(usage);
        return 0;
    }

    free(usage);

    m273_usage.last_input = (unsigned long)input_tokens;
    m273_usage.last_output = (unsigned long)output_tokens;
    m273_usage.last_total = (unsigned long)total_tokens;
    m273_usage.last_known = 1;

    if (m273_usage.requests != ULONG_MAX) {
        ++m273_usage.requests;
    }

    m273_usage.session_input = m273_usage_add(
        m273_usage.session_input,
        m273_usage.last_input
    );
    m273_usage.session_output = m273_usage_add(
        m273_usage.session_output,
        m273_usage.last_output
    );
    m273_usage.session_total = m273_usage_add(
        m273_usage.session_total,
        m273_usage.last_total
    );

    return 1;
}

int llm_usage_record_file(const char *path)
{
    char *json;
    int result;

    if (path == NULL || *path == '\0') {
        m273_usage.last_known = 0;
        return 0;
    }

    json = read_entire_file(path, NULL);
    if (json == NULL) {
        m273_usage.last_known = 0;
        return 0;
    }

    result = llm_usage_record_json(json);
    free(json);
    return result;
}

void llm_usage_get(llm_usage_stats *stats)
{
    if (stats != NULL) {
        *stats = m273_usage;
    }
}

int llm_usage_status(char *output,
                     size_t output_size,
                     unsigned int turn,
                     unsigned int limit)
{
    int written;

    if (output == NULL || output_size == 0U || limit == 0U) {
        return 0;
    }

    if (m273_usage.requests == 0UL) {
        written = snprintf(
            output,
            output_size,
            "turn %u/%u",
            turn,
            limit
        );
    } else if (m273_usage.last_known) {
        written = snprintf(
            output,
            output_size,
            "turn %u/%u | tok in=%lu out=%lu total=%lu | session=%lu",
            turn,
            limit,
            m273_usage.last_input,
            m273_usage.last_output,
            m273_usage.last_total,
            m273_usage.session_total
        );
    } else {
        written = snprintf(
            output,
            output_size,
            "turn %u/%u | tok last=n/a | session=%lu",
            turn,
            limit,
            m273_usage.session_total
        );
    }

    return written >= 0 && (size_t)written < output_size;
}
