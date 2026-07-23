#include <ctype.h>
#include <stddef.h>
#include <string.h>

#include "util.h"

void util_trim(char *text)
{
    size_t length;

    if (text == NULL) {
        return;
    }

    length = strlen(text);

    while (length != 0U) {
        unsigned char ch;

        ch = (unsigned char)text[length - 1U];

        if (ch != '\n' && ch != '\r' && !isspace(ch)) {
            break;
        }

        --length;
        text[length] = '\0';
    }
}

void util_uppercase(char *text)
{
    unsigned char *position;

    if (text == NULL) {
        return;
    }

    position = (unsigned char *)text;

    while (*position != (unsigned char)'\0') {
        *position = (unsigned char)toupper((int)*position);
        ++position;
    }
}

char *util_skip_space(char *text)
{
    if (text == NULL) {
        return NULL;
    }

    while (*text != '\0' && isspace((unsigned char)*text)) {
        ++text;
    }

    return text;
}
