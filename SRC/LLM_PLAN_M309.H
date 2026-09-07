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

static int llm_m309_response_complete(void)
{
    char *json;
    long output_tokens;
    long reasoning_tokens;
    int exhausted;

    json = read_entire_file(LLM_RESPONSE_FILE, NULL);
    if (json == NULL) {
        return 1;
    }

    output_tokens = 0L;
    reasoning_tokens = 0L;
    exhausted = llm_response_token_exhausted(
        json,
        &output_tokens,
        &reasoning_tokens);
    free(json);

    if (!exhausted) {
        return 1;
    }

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
    return 0;
}

static int llm_m309_plan_save(
    const char *goal,
    const char *plan_text)
{
    if (llm_last_workflow == LLM_WORKFLOW_PLAN) {
        if (!llm_m309_response_complete()) {
            return 0;
        }

        if (!llm_m309_plan_complete(plan_text)) {
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
