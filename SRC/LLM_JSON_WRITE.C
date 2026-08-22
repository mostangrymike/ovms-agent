#include <stdio.h>
#include <string.h>

#include "LLM_JSON_WRITE.H"

int json_write_escaped(FILE *file, const char *text)
{
    const unsigned char *position;

    if (file == NULL || text == NULL) {
        return 0;
    }

    for (position = (const unsigned char *)text;
         *position != (unsigned char)'\0';
         ++position) {
        switch (*position) {
        case '"':
            if (fputs("\\\"", file) == EOF) {
                return 0;
            }
            break;

        case '\\':
            if (fputs("\\\\", file) == EOF) {
                return 0;
            }
            break;

        case '\b':
            if (fputs("\\b", file) == EOF) {
                return 0;
            }
            break;

        case '\f':
            if (fputs("\\f", file) == EOF) {
                return 0;
            }
            break;

        case '\n':
            if (fputs("\\n", file) == EOF) {
                return 0;
            }
            break;

        case '\r':
            if (fputs("\\r", file) == EOF) {
                return 0;
            }
            break;

        case '\t':
            if (fputs("\\t", file) == EOF) {
                return 0;
            }
            break;

        default:
            if (*position < 32U) {
                if (fprintf(file,
                            "\\u%04x",
                            (unsigned int)*position) < 0) {
                    return 0;
                }
            } else if (fputc((int)*position, file) == EOF) {
                return 0;
            }
            break;
        }
    }

    return 1;
}
