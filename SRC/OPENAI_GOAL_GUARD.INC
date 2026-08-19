/*
 * Deterministic supervised-repair goal preservation checks.
 *
 * Production code includes this from OPENAI_AGENT_LOOP.C. Helpers remain
 * file-local, so the guard adds no linker-visible identifiers.
 */

static int goal_guard_code_token(const char *text, size_t length)
{
    size_t index;
    int saw_alpha;

    if (text == NULL || length < 3U) {
        return 0;
    }

    saw_alpha = 0;

    for (index = 0U; index < length; ++index) {
        unsigned char value;

        value = (unsigned char)text[index];

        if (isalpha(value)) {
            saw_alpha = 1;
            if (!isupper(value)) {
                return 0;
            }
        } else if (!isdigit(value) && value != '_' && value != '-') {
            return 0;
        }
    }

    return saw_alpha;
}

static int goal_guard_word_eq(const char *text,
                              size_t length,
                              const char *word)
{
    size_t index;

    if (text == NULL || word == NULL || strlen(word) != length) {
        return 0;
    }

    for (index = 0U; index < length; ++index) {
        if (toupper((unsigned char)text[index]) !=
            toupper((unsigned char)word[index])) {
            return 0;
        }
    }

    return 1;
}

static int goal_guard_remove_intent(const char *goal_start,
                                    const char *token_start)
{
    const char *cursor;
    const char *word_start;
    const char *word_end;
    const char *prior_start;
    const char *prior_end;
    size_t word_length;
    size_t prior_length;
    int destructive;

    if (goal_start == NULL || token_start == NULL ||
        token_start <= goal_start) {
        return 0;
    }

    cursor = token_start;
    while (cursor > goal_start &&
           !isalnum((unsigned char)cursor[-1]) &&
           cursor[-1] != '_' && cursor[-1] != '-') {
        --cursor;
    }

    word_end = cursor;
    while (cursor > goal_start &&
           isalpha((unsigned char)cursor[-1])) {
        --cursor;
    }
    word_start = cursor;
    word_length = (size_t)(word_end - word_start);

    destructive =
        goal_guard_word_eq(word_start, word_length, "REMOVE") ||
        goal_guard_word_eq(word_start, word_length, "DELETE") ||
        goal_guard_word_eq(word_start, word_length, "DISABLE") ||
        goal_guard_word_eq(word_start, word_length, "DROP") ||
        goal_guard_word_eq(word_start, word_length, "OMIT");

    if (!destructive) {
        return 0;
    }

    cursor = word_start;
    while (cursor > goal_start &&
           !isalnum((unsigned char)cursor[-1]) &&
           cursor[-1] != '_' && cursor[-1] != '-') {
        --cursor;
    }

    prior_end = cursor;
    while (cursor > goal_start &&
           isalpha((unsigned char)cursor[-1])) {
        --cursor;
    }
    prior_start = cursor;
    prior_length = (size_t)(prior_end - prior_start);

    if (goal_guard_word_eq(prior_start, prior_length, "NOT") ||
        goal_guard_word_eq(prior_start, prior_length, "NEVER") ||
        goal_guard_word_eq(prior_start, prior_length, "WITHOUT")) {
        return 0;
    }

    return 1;
}

static const char *goal_guard_goal_start(const char *goal,
                                         const char **goal_end)
{
    static const char user_label[] = "User goal:\n";
    static const char build_label[] =
        "\n\nCurrent failed build result:\n";
    const char *start;
    const char *end;

    if (goal == NULL) {
        if (goal_end != NULL) {
            *goal_end = NULL;
        }
        return NULL;
    }

    start = strstr(goal, user_label);
    if (start != NULL) {
        start += strlen(user_label);
        end = strstr(start, build_label);
        if (end == NULL) {
            end = goal + strlen(goal);
        }
    } else {
        start = goal;
        end = goal + strlen(goal);
    }

    if (goal_end != NULL) {
        *goal_end = end;
    }

    return start;
}

static int goal_guard_text_ok(const char *goal,
                              const char *old_text,
                              const char *new_text,
                              char *reason,
                              size_t reason_size)
{
    const char *cursor;
    const char *goal_start;
    const char *goal_end;

    if (reason != NULL && reason_size > 0U) {
        reason[0] = '\0';
    }

    if (goal == NULL || old_text == NULL || new_text == NULL) {
        return 1;
    }

    goal_start = goal_guard_goal_start(goal, &goal_end);
    cursor = goal_start;
    if (cursor == NULL || goal_end == NULL) {
        return 1;
    }

    while (cursor < goal_end) {
        const char *token_start;
        size_t token_length;

        while (cursor < goal_end &&
               !isalnum((unsigned char)*cursor) &&
               *cursor != '_' && *cursor != '-') {
            ++cursor;
        }

        token_start = cursor;

        while (cursor < goal_end &&
               (isalnum((unsigned char)*cursor) ||
                *cursor == '_' || *cursor == '-')) {
            ++cursor;
        }

        token_length = (size_t)(cursor - token_start);

        if (goal_guard_code_token(token_start, token_length) &&
            token_length < 128U &&
            !goal_guard_remove_intent(goal_start, token_start)) {
            char token[128];

            (void)memcpy(token, token_start, token_length);
            token[token_length] = '\0';

            if (strstr(old_text, token) != NULL &&
                strstr(new_text, token) == NULL) {
                if (reason != NULL && reason_size > 0U) {
                    (void)snprintf(
                        reason,
                        reason_size,
                        "Proposed repair removes goal term '%s' from the "
                        "source being replaced.",
                        token
                    );
                }
                return 0;
            }
        }
    }

    return 1;
}

#ifndef OPENAI_GOAL_GUARD_TEXT_ONLY

static char *goal_guard_line_text(const char *arguments)
{
    char *path;
    char *output;
    FILE *file;
    char line[2048];
    long first_line;
    long last_line;
    long line_number;
    size_t used;

    path = extract_string_argument(arguments, "path");

    if (path == NULL ||
        !extract_integer_argument(arguments, "first_line", &first_line) ||
        !extract_integer_argument(arguments, "last_line", &last_line)) {
        free(path);
        return NULL;
    }

    if (!llm_path_is_safe(path) ||
        llm_path_is_sensitive(path) ||
        first_line < 1L ||
        last_line < first_line ||
        last_line - first_line > 2000L) {
        free(path);
        return NULL;
    }

    file = fopen(path, "r");
    free(path);

    if (file == NULL) {
        return NULL;
    }

    output = (char *)malloc(OPENAI_RANGE_OUTPUT_LIMIT);
    if (output == NULL) {
        (void)fclose(file);
        return NULL;
    }

    output[0] = '\0';
    used = 0U;
    line_number = 1L;

    while (fgets(line, sizeof(line), file) != NULL) {
        size_t length;

        if (line_number > last_line) {
            break;
        }

        if (line_number >= first_line) {
            length = strlen(line);

            if (length >= OPENAI_RANGE_OUTPUT_LIMIT - used) {
                free(output);
                (void)fclose(file);
                return NULL;
            }

            (void)memcpy(output + used, line, length);
            used += length;
            output[used] = '\0';
        }

        ++line_number;
    }

    (void)fclose(file);

    if (line_number <= first_line) {
        free(output);
        return NULL;
    }

    return output;
}

static int goal_guard_args_ok(const char *goal,
                              const char *arguments,
                              int use_lines,
                              char *reason,
                              size_t reason_size)
{
    char *old_text;
    char *new_text;
    int allowed;

    if (arguments == NULL) {
        return 1;
    }

    old_text = use_lines ?
        goal_guard_line_text(arguments) :
        extract_string_argument(arguments, "old_text");
    new_text = extract_string_argument(arguments, "new_text");

    if (old_text == NULL || new_text == NULL) {
        free(old_text);
        free(new_text);
        return 1;
    }

    allowed = goal_guard_text_ok(
        goal,
        old_text,
        new_text,
        reason,
        reason_size
    );

    free(old_text);
    free(new_text);
    return allowed;
}

#endif /* OPENAI_GOAL_GUARD_TEXT_ONLY */
