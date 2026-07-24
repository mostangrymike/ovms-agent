#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "openai.h"

#define OPENAI_REQUEST_FILE  "OVMS_AGENT_REQUEST.JSON"
#define OPENAI_RESPONSE_FILE "OVMS_AGENT_RESPONSE.JSON"
#define OPENAI_HEADERS_FILE  "OVMS_AGENT_HEADERS.TXT"
#define OPENAI_LINE_SIZE     2048

static int json_write_escaped(FILE *file, const char *text)
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

static int write_request(const char *model, const char *prompt)
{
    FILE *file;
    int success;

    file = fopen(OPENAI_REQUEST_FILE, "w");

    if (file == NULL) {
        (void)printf("Unable to create %s: %s\n",
                     OPENAI_REQUEST_FILE,
                     strerror(errno));
        return 0;
    }

    success = 1;

    if (fputs("{\"model\":\"", file) == EOF ||
        !json_write_escaped(file, model) ||
        fputs("\",\"input\":\"", file) == EOF ||
        !json_write_escaped(file, prompt) ||
        fputs("\"}\n", file) == EOF) {
        success = 0;
    }

    if (fclose(file) != 0) {
        success = 0;
    }

    if (!success) {
        (void)puts("Unable to write complete API request.");
        return 0;
    }

    return 1;
}

static int write_headers(const char *api_key)
{
    FILE *file;
    int success;

    file = fopen(OPENAI_HEADERS_FILE, "w");

    if (file == NULL) {
        (void)printf("Unable to create %s: %s\n",
                     OPENAI_HEADERS_FILE,
                     strerror(errno));
        return 0;
    }

    success = 1;

    if (fputs("Content-Type: application/json\n", file) == EOF ||
        fputs("Authorization: Bearer ", file) == EOF ||
        fputs(api_key, file) == EOF ||
        fputc('\n', file) == EOF) {
        success = 0;
    }

    if (fclose(file) != 0) {
        success = 0;
    }

    if (!success) {
        (void)puts("Unable to write API headers.");
        return 0;
    }

    return 1;
}

static void remove_temporary_files(void)
{
    (void)remove(OPENAI_REQUEST_FILE);
    (void)remove(OPENAI_HEADERS_FILE);
}

static void display_response(void)
{
    FILE *file;
    char line[OPENAI_LINE_SIZE];

    file = fopen(OPENAI_RESPONSE_FILE, "r");

    if (file == NULL) {
        (void)printf("Unable to open %s: %s\n",
                     OPENAI_RESPONSE_FILE,
                     strerror(errno));
        return;
    }

    while (fgets(line, sizeof(line), file) != NULL) {
        (void)fputs(line, stdout);
    }

    if (ferror(file)) {
        (void)printf("\nUnable to read complete response: %s\n",
                     strerror(errno));
    }

    (void)fclose(file);
}

void openai_ask(agent_state *state, const char *prompt)
{
    const char *api_key;
    const char *model;
    int status;

    (void)state;

    if (prompt == NULL || *prompt == '\0') {
        (void)puts("Usage: ASK prompt");
        return;
    }

    api_key = getenv("OPENAI_API_KEY");

    if (api_key == NULL || *api_key == '\0') {
        (void)puts("OPENAI_API_KEY is not defined.");
        return;
    }

    model = getenv("OVMS_AGENT_MODEL");

    if (model == NULL || *model == '\0') {
        (void)puts("OVMS_AGENT_MODEL is not defined.");
        (void)puts("Define it to an API model available to the project.");
        return;
    }

    if (!write_request(model, prompt) ||
        !write_headers(api_key)) {
        remove_temporary_files();
        return;
    }

    (void)puts("Sending request to OpenAI...");

    status = system(
        "curl --silent --show-error "
        "--output " OPENAI_RESPONSE_FILE " "
        "--header @" OPENAI_HEADERS_FILE " "
        "--data-binary @" OPENAI_REQUEST_FILE " "
        "https://api.openai.com/v1/responses"
    );

    remove_temporary_files();

    if ((status & 1) == 0) {
        (void)printf(
            "curl failed with OpenVMS status %d.\n",
            status
        );
        return;
    }

    display_response();
    (void)putchar('\n');
}
