#include <ctype.h>
#include <string.h>

#include "openai_path.h"

int openai_path_is_safe(const char *path)
{
    if (path == NULL || *path == '\0') {
        return 0;
    }

    if (*path == '/' ||
        strchr(path, ':') != NULL ||
        strstr(path, "..") != NULL) {
        return 0;
    }

    return 1;
}

int openai_contains_ignore_case(const char *text,
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

int openai_path_is_sensitive(const char *path)
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
        if (openai_contains_ignore_case(path, *pattern)) {
            return 1;
        }
    }

    return 0;
}

int openai_listing_entry_hidden(const char *name)
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
        if (openai_contains_ignore_case(name, *pattern)) {
            return 1;
        }
    }

    length = strlen(name);

    if (length >= 4U) {
        const char *extension;

        extension = name + length - 4U;

        if (openai_contains_ignore_case(extension, ".OBJ") ||
            openai_contains_ignore_case(extension, ".EXE") ||
            openai_contains_ignore_case(extension, ".BAK") ||
            openai_contains_ignore_case(extension, ".OLD")) {
            return 1;
        }
    }

    return 0;
}
