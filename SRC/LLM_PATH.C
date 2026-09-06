#include <ctype.h>
#include <string.h>

#include "llm_path.h"

static int llm_vms_dir_is_safe(const char *path)
{
    const char *square;
    const char *angle;
    const char *open;
    const char *close;
    const char *cursor;
    char close_char;

    square = strchr(path, '[');
    angle = strchr(path, '<');

    if (square != NULL && angle != NULL) {
        return 0;
    }

    open = square != NULL ? square : angle;
    if (open == NULL) {
        return 1;
    }

    if (open != path) {
        return 0;
    }

    close_char = *open == '[' ? ']' : '>';
    close = strchr(open + 1, close_char);
    if (close == NULL ||
        strchr(close + 1, *open) != NULL ||
        strchr(close + 1, close_char) != NULL) {
        return 0;
    }

    if (open + 1 == close) {
        return 1;
    }

    if (open[1] != '.') {
        return 0;
    }

    cursor = open + 2;
    while (cursor < close) {
        const char *component;

        component = cursor;
        while (cursor < close && *cursor != '.') {
            ++cursor;
        }

        if (component == cursor ||
            (*component == '-' && component + 1 == cursor)) {
            return 0;
        }

        if (cursor < close) {
            ++cursor;
        }
    }

    return 1;
}

int llm_path_is_safe(const char *path)
{
    if (path == NULL || *path == '\0') {
        return 0;
    }

    if (*path == '/' ||
        strchr(path, ':') != NULL ||
        strstr(path, "..") != NULL ||
        !llm_vms_dir_is_safe(path)) {
        return 0;
    }

    return 1;
}

int llm_contains_ignore_case(const char *text,
                             const char *pattern)
{
    const unsigned char *start;

    if (text == NULL || pattern == NULL || *pattern == '\0') {
        return 0;
    }

    for (start = (const unsigned char *)text;
         *start != (unsigned char)'\0';
         ++start) {
        const unsigned char *left;
        const unsigned char *right;

        left = start;
        right = (const unsigned char *)pattern;

        while (*left != (unsigned char)'\0' &&
               *right != (unsigned char)'\0' &&
               tolower((int)*left) == tolower((int)*right)) {
            ++left;
            ++right;
        }

        if (*right == (unsigned char)'\0') {
            return 1;
        }
    }

    return 0;
}

int llm_path_is_sensitive(const char *path)
{
    static const char *blocked[] = {
        "OPENAIKEY",
        "OPENAI_API_KEY",
        "OVMS_AGENT_HEADERS",
        "OPENAI_TEST_HEADERS",
        ".PEM",
        ".KEY",
        NULL
    };
    const char **pattern;

    if (path == NULL) {
        return 1;
    }

    for (pattern = blocked; *pattern != NULL; ++pattern) {
        if (llm_contains_ignore_case(path, *pattern)) {
            return 1;
        }
    }

    return 0;
}

int llm_listing_entry_hidden(const char *name)
{
    static const char *hidden[] = {
        "OPENAIKEY",
        "OVMS_AGENT_HEADERS",
        "OVMS_AGENT_REQUEST.JSON",
        "OVMS_AGENT_RESPONSE.JSON",
        "OPENAI_MODELS.JSON",
        "_BACKUP",
        "_BEFORE_",
        NULL
    };
    const char **pattern;
    size_t length;

    if (name == NULL) {
        return 1;
    }

    for (pattern = hidden; *pattern != NULL; ++pattern) {
        if (llm_contains_ignore_case(name, *pattern)) {
            return 1;
        }
    }

    length = strlen(name);

    if (length >= 4U) {
        const char *extension;

        extension = name + length - 4U;

        if (llm_contains_ignore_case(extension, ".OBJ") ||
            llm_contains_ignore_case(extension, ".EXE") ||
            llm_contains_ignore_case(extension, ".BAK") ||
            llm_contains_ignore_case(extension, ".OLD")) {
            return 1;
        }
    }

    return 0;
}