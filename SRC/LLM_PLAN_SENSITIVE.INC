/*
 * Plan persistence secret-value screening.
 *
 * Mentions of credential-related names are allowed because implementation
 * plans may legitimately quote source code, diagnostics, filenames, and
 * documentation. Reject only value-bearing credential forms.
 */

static int plan_ci_match(const char *text, const char *pattern)
{
    size_t index;

    if (text == NULL || pattern == NULL) {
        return 0;
    }

    for (index = 0U; pattern[index] != '\0'; ++index) {
        if (text[index] == '\0' ||
            toupper((unsigned char)text[index]) !=
                toupper((unsigned char)pattern[index])) {
            return 0;
        }
    }

    return 1;
}

static const char *plan_skip_space(const char *text)
{
    while (text != NULL &&
           (*text == ' ' || *text == '\t')) {
        ++text;
    }

    return text;
}

static int plan_bearer_value(const char *text)
{
    static const char marker[] = "AUTHORIZATION: BEARER";
    const char *position;
    size_t marker_length;

    marker_length = strlen(marker);
    position = text;

    while (position != NULL && *position != '\0') {
        if (plan_ci_match(position, marker)) {
            const char *value;

            value = plan_skip_space(position + marker_length);

            if (value != NULL &&
                *value != '\0' &&
                *value != '\n' &&
                *value != '\r' &&
                *value != '"' &&
                *value != '\'' &&
                *value != ',' &&
                *value != ';' &&
                *value != ')' &&
                *value != ']' &&
                *value != '}' &&
                *value != '\\') {
                return 1;
            }
        }

        ++position;
    }

    return 0;
}

static int plan_api_key_value(const char *text)
{
    static const char marker[] = "OPENAI_API_KEY";
    const char *position;
    size_t marker_length;

    marker_length = strlen(marker);
    position = text;

    while (position != NULL && *position != '\0') {
        if (plan_ci_match(position, marker)) {
            const char *value;

            value = plan_skip_space(position + marker_length);

            if (value != NULL &&
                (*value == '=' || *value == ':')) {
                ++value;
                value = plan_skip_space(value);

                if (value != NULL &&
                    (*value == '"' || *value == '\'')) {
                    ++value;
                }

                if (value != NULL &&
                    plan_ci_match(value, "SK-")) {
                    return 1;
                }
            }
        }

        ++position;
    }

    return 0;
}

static int plan_has_sensitive_text(const char *text)
{
    if (text == NULL) {
        return 1;
    }

    return plan_api_key_value(text) ||
           plan_bearer_value(text);
}

#include "LLM_PLAN_SCOPE_M279.INC"
