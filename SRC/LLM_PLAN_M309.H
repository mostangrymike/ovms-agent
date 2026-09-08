#ifndef LLM_PLAN_M309_H
#define LLM_PLAN_M309_H

static int llm_m309_heading_line(
    const char *start,
    size_t length,
    const char *heading)
{
    size_t heading_length;
    size_t index;

    if (start == NULL || heading == NULL) {
        return 0;
    }

    index = 0U;
    while (index < length &&
           (start[index] == ' ' || start[index] == '\t')) {
        ++index;
    }

    while (index < length && start[index] == '#') {
        ++index;
    }
    while (index < length &&
           (start[index] == ' ' || start[index] == '\t')) {
        ++index;
    }

    heading_length = strlen(heading);
    if (length - index < heading_length ||
        memcmp(start + index, heading, heading_length) != 0) {
        return 0;
    }

    index += heading_length;
    while (index < length &&
           (start[index] == ' ' || start[index] == '\t')) {
        ++index;
    }

    if (index < length && start[index] == ':') {
        ++index;
        while (index < length &&
               (start[index] == ' ' || start[index] == '\t')) {
            ++index;
        }
    }

    return index == length;
}

static int llm_m309_line_exact(
    const char *start,
    size_t length,
    const char *text)
{
    size_t text_length;
    size_t index;

    if (start == NULL || text == NULL) {
        return 0;
    }

    index = 0U;
    while (index < length &&
           (start[index] == ' ' || start[index] == '\t')) {
        ++index;
    }

    text_length = strlen(text);
    return length - index == text_length &&
        memcmp(start + index, text, text_length) == 0;
}

static int llm_m309_plan_complete(const char *plan_text)
{
    static const char *headings[] = {
        "Goal",
        "Files to inspect",
        "Files to modify",
        "Files to create",
        "Ordered edits",
        "Validation",
        "Risks",
        "Authority required",
        NULL
    };
    const char *cursor;
    const char *line_start;
    unsigned int found;
    int saw_count;
    int saw_advisory;

    if (plan_text == NULL || *plan_text == '\0') {
        return 0;
    }

    found = 0U;
    saw_count = 0;
    saw_advisory = 0;
    line_start = plan_text;
    cursor = plan_text;

    for (;;) {
        if (*cursor == '\n' || *cursor == '\0') {
            const char *content_end;
            size_t content_length;
            unsigned int index;

            content_end = cursor;
            if (content_end > line_start &&
                content_end[-1] == '\r') {
                --content_end;
            }
            content_length = (size_t)(content_end - line_start);

            for (index = 0U; headings[index] != NULL; ++index) {
                if ((found & (1U << index)) == 0U &&
                    llm_m309_heading_line(
                        line_start,
                        content_length,
                        headings[index])) {
                    found |= (1U << index);
                }
            }

            if (content_length >= 16U &&
                memcmp(line_start, "operation_count=", 16U) == 0) {
                saw_count = 1;
            }

            if (llm_m309_line_exact(
                    line_start,
                    content_length,
                    "Plan mode: advisory-only")) {
                saw_advisory = 1;
            }

            if (*cursor == '\0') {
                break;
            }
            ++cursor;
            line_start = cursor;
        } else {
            ++cursor;
        }
    }

    if (found != 0xffU) {
        return 0;
    }

    return saw_count || saw_advisory;
}

static char *llm_m309_read_response(void)
{
    FILE *file;
    long length;
    size_t count;
    char *text;

    file = fopen(LLM_RESPONSE_FILE, "r");
    if (file == NULL) {
        return NULL;
    }

    if (fseek(file, 0L, SEEK_END) != 0) {
        (void)fclose(file);
        return NULL;
    }

    length = ftell(file);
    if (length < 0L ||
        length > 262144L ||
        fseek(file, 0L, SEEK_SET) != 0) {
        (void)fclose(file);
        return NULL;
    }

    text = (char *)malloc((size_t)length + 1U);
    if (text == NULL) {
        (void)fclose(file);
        return NULL;
    }

    count = fread(text, 1U, (size_t)length, file);
    if (fclose(file) != 0 || count != (size_t)length) {
        free(text);
        return NULL;
    }

    text[count] = '\0';
    return text;
}

static int llm_m309_response_has_text(
    const char *json,
    const char *plan_text)
{
#ifdef M309_PLAN_TEST
    if (response_text_fixture != NULL) {
        return plan_text != NULL &&
            strcmp(response_text_fixture, plan_text) == 0;
    }
#endif
    {
        const unsigned char *cursor;
        char *encoded;
        char *output;
        size_t length;
        size_t capacity;
        size_t used;
        int result;

        if (json == NULL || plan_text == NULL) {
            return 0;
        }

        length = strlen(plan_text);
        if (length > (((size_t)-1) - 3U) / 6U) {
            return 0;
        }
        capacity = length * 6U + 3U;
        encoded = (char *)malloc(capacity);
        if (encoded == NULL) {
            return 0;
        }

        output = encoded;
        *output++ = '"';
        used = 1U;
        cursor = (const unsigned char *)plan_text;

        while (*cursor != '\0') {
            const char *escape;
            char hex[7];
            size_t escape_length;

            escape = NULL;
            escape_length = 0U;

            if (*cursor == '"') {
                escape = "\\\"";
            } else if (*cursor == '\\') {
                escape = "\\\\";
            } else if (*cursor == '\b') {
                escape = "\\b";
            } else if (*cursor == '\f') {
                escape = "\\f";
            } else if (*cursor == '\n') {
                escape = "\\n";
            } else if (*cursor == '\r') {
                escape = "\\r";
            } else if (*cursor == '\t') {
                escape = "\\t";
            } else if (*cursor < 0x20U) {
                (void)snprintf(hex, sizeof(hex), "\\u%04X",
                               (unsigned int)*cursor);
                escape = hex;
            }

            if (escape != NULL) {
                escape_length = strlen(escape);
                if (used + escape_length + 2U > capacity) {
                    free(encoded);
                    return 0;
                }
                (void)memcpy(output, escape, escape_length);
                output += escape_length;
                used += escape_length;
            } else {
                if (used + 3U > capacity) {
                    free(encoded);
                    return 0;
                }
                *output++ = (char)*cursor;
                ++used;
            }
            ++cursor;
        }

        *output++ = '"';
        *output = '\0';
        result = strstr(json, encoded) != NULL;
        free(encoded);
        return result;
    }
}

static long llm_m309_json_long(
    const char *json,
    const char *name)
{
    const char *position;
    char pattern[64];
    char *end;
    long value;
    int count;

    if (json == NULL || name == NULL) {
        return 0L;
    }

    count = snprintf(
        pattern,
        sizeof(pattern),
        "\"%s\":",
        name);
    if (count <= 0 || (size_t)count >= sizeof(pattern)) {
        return 0L;
    }

    position = strstr(json, pattern);
    if (position == NULL) {
        return 0L;
    }

    position += strlen(pattern);
    while (*position == ' ' || *position == '\t') {
        ++position;
    }

    value = strtol(position, &end, 10);
    if (end == position || value < 0L) {
        return 0L;
    }

    return value;
}

static int llm_m309_response_state(const char *plan_text)
{
    char *json;
    long output_tokens;
    long reasoning_tokens;
    int incomplete;
    int completed;

    json = llm_m309_read_response();
    if (json == NULL) {
        return 0;
    }

    if (!llm_m309_response_has_text(json, plan_text)) {
        free(json);
        return 0;
    }

    incomplete =
        strstr(json, "\"status\":\"incomplete\"") != NULL &&
        strstr(json, "\"incomplete_details\"") != NULL &&
        strstr(json, "\"reason\":\"max_output_tokens\"") != NULL;
    completed =
        strstr(json, "\"status\":\"completed\"") != NULL;

    if (!incomplete) {
        free(json);
        return completed ? 1 : 0;
    }

    output_tokens = llm_m309_json_long(json, "output_tokens");
    reasoning_tokens = llm_m309_json_long(json, "reasoning_tokens");
    free(json);

    (void)puts(
        "Implementation plan was not saved because the provider response "
        "was incomplete at the output-token limit. Increase "
        "max_output_tokens and retry.");
    if (output_tokens > 0L || reasoning_tokens > 0L) {
        (void)printf(
            "Provider usage reported output_tokens=%ld, reasoning_tokens=%ld.\n",
            output_tokens,
            reasoning_tokens);
    }
    return -1;
}

static int llm_m309_plan_save(
    const char *goal,
    const char *plan_text)
{
    int response_state;

    if (llm_last_workflow == LLM_WORKFLOW_PLAN) {
        response_state = llm_m309_response_state(plan_text);
        if (response_state < 0) {
            return 0;
        }

        if (response_state > 0 &&
            !llm_m309_plan_complete(plan_text)) {
            (void)puts(
                "Implementation plan was not saved because the provider "
                "did not return a complete plan.");
            return 0;
        }
    }

    return llm_plan_save(goal, plan_text);
}

#define llm_plan_save llm_m309_plan_save

#endif /* LLM_PLAN_M309_H */
