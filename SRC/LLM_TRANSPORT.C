#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "llm_internal.h"
#include "LLM_TRANSPORT.H"
#include "llm_config.h"

#define M262_RESPONSE_URL_MAX 640U

static int m262_response_url(const char *base,
                             char *output,
                             size_t output_size)
{
    static const char suffix[] = "/responses";
    size_t length;
    size_t suffix_length;

    if (base == NULL || output == NULL || output_size == 0U) {
        return 0;
    }

    length = strlen(base);
    while (length > 0U && base[length - 1U] == '/') {
        --length;
    }

    suffix_length = strlen(suffix);
    if (length >= suffix_length &&
        strncmp(base + length - suffix_length,
                suffix, suffix_length) == 0) {
        if (length + 1U > output_size) {
            return 0;
        }
        (void)memcpy(output, base, length);
        output[length] = '\0';
        return 1;
    }

    if (length + suffix_length + 1U > output_size) {
        return 0;
    }

    (void)memcpy(output, base, length);
    (void)memcpy(output + length, suffix, suffix_length + 1U);
    return 1;
}

static int m262_url_valid(const char *url)
{
    const unsigned char *cursor;

    if (url == NULL ||
        (strncmp(url, "https://", 8U) != 0 &&
         strncmp(url, "http://", 7U) != 0)) {
        return 0;
    }

    cursor = (const unsigned char *)url;
    while (*cursor != '\0') {
        if (!(isalnum(*cursor) ||
              *cursor == ':' || *cursor == '/' || *cursor == '.' ||
              *cursor == '_' || *cursor == '-' || *cursor == '~')) {
            return 0;
        }
        ++cursor;
    }

    return 1;
}

int llm_write_headers(const char *api_key)
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
        (void)puts("Unable to write service headers.");
        return 0;
    }

    return 1;
}

void llm_remove_temp_files(void)
{
    (void)remove(OPENAI_REQUEST_FILE);
    (void)remove(OPENAI_HEADERS_FILE);
}

int llm_perform_request(void)
{
    static const char prefix[] =
        "curl --silent --show-error "
        "--output " OPENAI_RESPONSE_FILE " "
        "--header @" OPENAI_HEADERS_FILE " "
        "--data-binary @" OPENAI_REQUEST_FILE " ";
    const char *base;
    char url[M262_RESPONSE_URL_MAX];
    char command[1024];
    size_t needed;
    int status;

    base = llm_api_url();
    if (base == NULL || *base == '\0') {
        (void)puts(
            "No AI provider service address is configured. "
            "Use PROVIDER ADD or PROVIDER USE."
        );
        return 0;
    }

    if (!m262_url_valid(base) ||
        !m262_response_url(base, url, sizeof(url))) {
        (void)puts("Configured AI provider service address is invalid.");
        return 0;
    }

    needed = strlen(prefix) + strlen(url) + 1U;
    if (needed > sizeof(command)) {
        (void)puts("Configured AI provider service address is too long.");
        return 0;
    }

    (void)strcpy(command, prefix);
    (void)strcat(command, url);
    status = system(command);

    return (status & 1) != 0;
}
