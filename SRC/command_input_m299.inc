/*
 * M299 native .END multiline command entry.
 *
 * This layer decides only when the interactive command reader should collect
 * a free-form prompt/goal body.  The M251 reader remains authoritative for
 * .END termination, embedded-newline preservation, overflow draining, and
 * EOF behavior.
 *
 * Normal one-line commands remain immediate.  Multiline collection begins
 * only when an eligible command has all of its structured leading arguments
 * but no free-form tail yet.
 */

#include <ctype.h>
#include <string.h>

typedef struct main_m299_rule {
    const char *name;
    unsigned int fixed_arguments;
} main_m299_rule;

static const main_m299_rule main_m299_rules[] = {
    { "ASK", 0U },
    { "CHAT", 0U },
    { "AGENT", 0U },
    { "AGENT/WRITE", 0U },
    { "AGENT/BUILD", 0U },
    { "AGENT/FIX", 0U },
    { "AGENT/RETRY", 0U },
    { "AGENT/REPAIR", 0U },
    { "AGENT/CREATE", 0U },
    { "AGENT/PLAN", 0U },
    { "AGENT/EXEC", 0U },
    { "AGENT/EXEC/DRY", 0U },
    { "AGENT/EXEC/RESUME", 0U },
    { "AGENT/AUTOPILOT", 0U },
    { "AGENT/EXEC/RESUME/CREATE", 0U },
    { "AGENT/IMAGE", 1U },
    { "AGENT/SESSION/EXEC", 1U }
};

static int main_m299_equal_ci_n(const char *left,
                                size_t left_length,
                                const char *right)
{
    size_t index;

    if (left == NULL || right == NULL ||
        strlen(right) != left_length) {
        return 0;
    }

    for (index = 0U; index < left_length; ++index) {
        if (toupper((unsigned char)left[index]) !=
            toupper((unsigned char)right[index])) {
            return 0;
        }
    }

    return 1;
}

static int main_m299_equal_ci(const char *left, const char *right)
{
    if (left == NULL || right == NULL) {
        return 0;
    }

    return main_m299_equal_ci_n(left, strlen(left), right);
}

static const char *main_m299_skip_ws(const char *text)
{
    if (text == NULL) {
        return NULL;
    }

    while (*text == ' ' || *text == '\t') {
        ++text;
    }

    return text;
}

static const char *main_m299_skip_arg(const char *text)
{
    int quoted;

    text = main_m299_skip_ws(text);
    if (text == NULL || *text == '\0') {
        return NULL;
    }

    quoted = *text == '"';
    if (quoted) {
        ++text;
        while (*text != '\0' && *text != '"') {
            if (*text == '\\' && text[1] != '\0') {
                text += 2;
            } else {
                ++text;
            }
        }

        if (*text != '"') {
            return NULL;
        }

        return text + 1;
    }

    while (*text != '\0' && *text != ' ' && *text != '\t') {
        ++text;
    }

    return text;
}

static int main_m299_copy(char *prefix,
                          size_t prefix_size,
                          const char *start,
                          size_t length)
{
    if (prefix == NULL || start == NULL ||
        prefix_size == 0U || length + 1U > prefix_size) {
        return 0;
    }

    if (length > 0U) {
        (void)memcpy(prefix, start, length);
    }
    prefix[length] = '\0';
    return 1;
}

static int main_m299_alias(const char *input,
                           char *prefix,
                           size_t prefix_size)
{
    if (main_m299_equal_ci(input, "AGENT/MULTILINE")) {
        return main_m299_copy(prefix, prefix_size,
                              "AGENT", strlen("AGENT"));
    }

    if (main_m299_equal_ci(input, "AGENT/CREATE/MULTILINE")) {
        return main_m299_copy(prefix, prefix_size,
                              "AGENT/CREATE", strlen("AGENT/CREATE"));
    }

    if (main_m299_equal_ci(input, "AGENT/REPAIR/MULTILINE")) {
        return main_m299_copy(prefix, prefix_size,
                              "AGENT/REPAIR", strlen("AGENT/REPAIR"));
    }

    return 0;
}

static int main_m299_fork(const char *command_start,
                          const char *command_end,
                          char *prefix,
                          size_t prefix_size)
{
    const char *cursor;
    const char *divider;
    const char *name_start;
    const char *name_end;
    const char *tail;

    if (!main_m299_equal_ci_n(
            command_start,
            (size_t)(command_end - command_start),
            "AGENT/EXEC/FORK")) {
        return 0;
    }

    cursor = main_m299_skip_ws(command_end);
    if (cursor == NULL || *cursor == '\0') {
        return 0;
    }

    divider = strstr(cursor, "::");
    if (divider == NULL) {
        return 0;
    }

    name_start = cursor;
    name_end = divider;
    while (name_end > name_start &&
           (name_end[-1] == ' ' || name_end[-1] == '\t')) {
        --name_end;
    }

    if (name_end == name_start) {
        return 0;
    }

    tail = main_m299_skip_ws(divider + 2);
    if (tail == NULL || *tail != '\0') {
        return 0;
    }

    return main_m299_copy(
        prefix,
        prefix_size,
        command_start,
        (size_t)((divider + 2) - command_start)
    );
}

static int main_multiline_prefix(char *input,
                                 char *prefix,
                                 size_t prefix_size)
{
    const char *command_start;
    const char *command_end;
    const char *cursor;
    const char *argument_end;
    size_t length;
    size_t index;
    unsigned int argument;

    if (input == NULL || prefix == NULL || prefix_size == 0U) {
        return 0;
    }

    prefix[0] = '\0';
    length = strlen(input);

    while (length > 0U &&
           (input[length - 1U] == '\n' ||
            input[length - 1U] == '\r')) {
        input[--length] = '\0';
    }

    if (main_m299_alias(input, prefix, prefix_size)) {
        return 1;
    }

    command_start = main_m299_skip_ws(input);
    if (command_start == NULL || *command_start == '\0') {
        return 0;
    }

    command_end = command_start;
    while (*command_end != '\0' &&
           *command_end != ' ' &&
           *command_end != '\t') {
        ++command_end;
    }

    if (main_m299_fork(
            command_start,
            command_end,
            prefix,
            prefix_size)) {
        return 1;
    }

    for (index = 0U;
         index < sizeof(main_m299_rules) / sizeof(main_m299_rules[0]);
         ++index) {
        const main_m299_rule *rule;

        rule = &main_m299_rules[index];
        if (!main_m299_equal_ci_n(
                command_start,
                (size_t)(command_end - command_start),
                rule->name)) {
            continue;
        }

        cursor = command_end;
        argument_end = command_end;

        for (argument = 0U;
             argument < rule->fixed_arguments;
             ++argument) {
            cursor = main_m299_skip_ws(cursor);
            if (cursor == NULL || *cursor == '\0') {
                return 0;
            }

            argument_end = main_m299_skip_arg(cursor);
            if (argument_end == NULL) {
                return 0;
            }

            cursor = argument_end;
        }

        cursor = main_m299_skip_ws(cursor);
        if (cursor == NULL || *cursor != '\0') {
            return 0;
        }

        return main_m299_copy(
            prefix,
            prefix_size,
            command_start,
            (size_t)(argument_end - command_start)
        );
    }

    return 0;
}
