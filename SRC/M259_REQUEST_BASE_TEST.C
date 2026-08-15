#include <stdio.h>
#include <string.h>

#include "openai_internal.h"
#include "openai_request_basic.h"

static int m259_read_request(char *output, size_t output_size)
{
    FILE *file;
    size_t used;
    int ch;

    if (output == NULL || output_size == 0U) return 0;
    file = fopen(OPENAI_REQUEST_FILE, "r");
    if (file == NULL) return 0;

    used = 0U;
    while ((ch = fgetc(file)) != EOF) {
        if (used + 1U >= output_size) {
            (void)fclose(file);
            return 0;
        }
        output[used++] = (char)ch;
    }
    output[used] = '\0';
    return fclose(file) == 0;
}

int main(void)
{
    char request[4096];

    (void)remove(OPENAI_REQUEST_FILE);
    if (!write_request("m259-test-model", "Inspect SRC/MAIN.C", NULL)) {
        (void)puts("M259 request baseline failed: write_request.");
        return 2;
    }
    if (!m259_read_request(request, sizeof(request))) {
        (void)puts("M259 request baseline failed: read request.");
        (void)remove(OPENAI_REQUEST_FILE);
        return 2;
    }

    if (strstr(request, "\"model\":\"m259-test-model\"") == NULL ||
        strstr(request, "\"input\":\"Inspect SRC/MAIN.C\"") == NULL ||
        strstr(request, "input_image") != NULL ||
        strstr(request, "parallel_tool_calls\":false") == NULL) {
        (void)puts("M259 request baseline failed: unexpected serialization.");
        (void)remove(OPENAI_REQUEST_FILE);
        return 2;
    }

    (void)remove(OPENAI_REQUEST_FILE);
    (void)puts("M259 text-only request baseline regression passed.");
    return 1;
}
