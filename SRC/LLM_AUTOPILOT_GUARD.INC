/* M268 adapter between saved implementation plans and the existing
 * goal_guard_text_ok() primitive.  Marker matching mirrors the executor's
 * line-oriented format so marker-looking source text cannot hide an edit.
 */
#define LLM_GOAL_GUARD_TEXT_ONLY 1
#include "LLM_GOAL_GUARD.INC"
#undef LLM_GOAL_GUARD_TEXT_ONLY

static const char *m268_find_text(const char *start,
                                  const char *end,
                                  const char *needle)
{
    const char *found;
    size_t length;

    if (start == NULL || end == NULL || needle == NULL || end < start) {
        return NULL;
    }

    found = strstr(start, needle);
    if (found == NULL || found >= end) {
        return NULL;
    }

    length = strlen(needle);
    if (length > (size_t)(end - found)) {
        return NULL;
    }

    return found;
}

static const char *m268_find_marker(const char *start,
                                    const char *end,
                                    const char *marker)
{
    const char *cursor;
    const char *line_end;
    const char *trimmed_end;
    size_t marker_length;

    if (start == NULL || end == NULL || marker == NULL || end < start) {
        return NULL;
    }

    marker_length = strlen(marker);
    cursor = start;

    while (cursor < end) {
        line_end = cursor;
        while (line_end < end && *line_end != '\n') {
            ++line_end;
        }

        trimmed_end = line_end;
        if (trimmed_end > cursor && trimmed_end[-1] == '\r') {
            --trimmed_end;
        }

        if ((size_t)(trimmed_end - cursor) == marker_length &&
            memcmp(cursor, marker, marker_length) == 0) {
            return cursor;
        }

        if (line_end >= end) {
            break;
        }
        cursor = line_end + 1;
    }

    return NULL;
}

static char *m268_copy_text(const char *start, const char *end)
{
    char *copy;
    size_t length;

    if (start == NULL || end == NULL || end < start) {
        return NULL;
    }

    length = (size_t)(end - start);
    copy = (char *)malloc(length + 1U);
    if (copy == NULL) {
        return NULL;
    }

    if (length > 0U) {
        (void)memcpy(copy, start, length);
    }
    copy[length] = '\0';
    return copy;
}

static int m268_guard_block(const char *op_start,
                            const char *op_end,
                            const char *begin_marker,
                            const char *end_marker,
                            char **value_out)
{
    const char *begin;
    const char *content;
    const char *end;
    const char *line_end;

    if (value_out == NULL) {
        return 0;
    }
    *value_out = NULL;

    begin = m268_find_marker(op_start, op_end, begin_marker);
    if (begin == NULL) {
        return 0;
    }

    line_end = begin;
    while (line_end < op_end && *line_end != '\n') {
        ++line_end;
    }
    if (line_end >= op_end) {
        return 0;
    }
    content = line_end + 1;

    end = m268_find_marker(content, op_end, end_marker);
    if (end == NULL) {
        return 0;
    }

    while (end > content &&
           (end[-1] == '\n' || end[-1] == '\r')) {
        --end;
    }

    *value_out = m268_copy_text(content, end);
    return *value_out != NULL;
}

static int m268_guard_legacy(const char *op_start,
                             const char *op_end,
                             const char *name,
                             char **value_out)
{
    char needle[64];
    const char *value;
    const char *line_end;
    int written;

    if (name == NULL || value_out == NULL) {
        return 0;
    }
    *value_out = NULL;

    written = snprintf(needle, sizeof(needle), "\n%s=", name);
    if (written < 0 || (size_t)written >= sizeof(needle)) {
        return 0;
    }

    value = m268_find_text(op_start, op_end, needle);
    if (value == NULL) {
        return 0;
    }
    value += strlen(needle);

    line_end = value;
    while (line_end < op_end &&
           *line_end != '\n' && *line_end != '\r') {
        ++line_end;
    }

    *value_out = m268_copy_text(value, line_end);
    return *value_out != NULL;
}

static int m268_goal_guard_plan(const char *goal,
                                char *reason,
                                size_t reason_size)
{
    char *saved;
    const char *cursor;
    const char *plan_end;
    const char *op_start;
    const char *op_end;
    const char *replace_type;
    char *old_text;
    char *new_text;
    int allowed;

    if (reason != NULL && reason_size > 0U) {
        reason[0] = '\0';
    }

    saved = llm_read_text_file(M268_PLAN_FILE);
    if (saved == NULL) {
        return 1;
    }

    cursor = saved;
    plan_end = saved + strlen(saved);
    allowed = 1;

    while ((op_start = m268_find_marker(
                cursor, plan_end, "BEGIN_OPERATION")) != NULL) {
        op_end = m268_find_marker(
            op_start, plan_end, "END_OPERATION");
        if (op_end == NULL) {
            allowed = 0;
            if (reason != NULL && reason_size > 0U) {
                (void)snprintf(
                    reason, reason_size,
                    "Goal guard could not inspect an incomplete repair operation."
                );
            }
            break;
        }

        replace_type = m268_find_text(
            op_start, op_end, "\ntype=replace_");

        if (replace_type != NULL) {
            old_text = NULL;
            new_text = NULL;

            if (m268_find_marker(
                    op_start, op_end, "BEGIN_OLD_TEXT") != NULL) {
                allowed =
                    m268_guard_block(
                        op_start, op_end,
                        "BEGIN_OLD_TEXT", "END_OLD_TEXT", &old_text) &&
                    m268_guard_block(
                        op_start, op_end,
                        "BEGIN_NEW_TEXT", "END_NEW_TEXT", &new_text);
            } else {
                allowed =
                    m268_guard_legacy(
                        op_start, op_end, "old_text", &old_text) &&
                    m268_guard_legacy(
                        op_start, op_end, "new_text", &new_text);
            }

            if (!allowed || old_text == NULL || new_text == NULL) {
                if (reason != NULL && reason_size > 0U) {
                    (void)snprintf(
                        reason, reason_size,
                        "Goal guard could not inspect a replacement repair operation."
                    );
                }
                free(old_text);
                free(new_text);
                break;
            }

            allowed = goal_guard_text_ok(
                goal, old_text, new_text, reason, reason_size);
            free(old_text);
            free(new_text);

            if (!allowed) {
                break;
            }
        }

        cursor = op_end + strlen("END_OPERATION");
    }

    free(saved);
    return allowed;
}

/* Focused regression hook; production uses the static adapter above. */
int llm_m268_guard_test(const char *goal,
                        char *reason,
                        size_t reason_size)
{
    return m268_goal_guard_plan(goal, reason, reason_size);
}
