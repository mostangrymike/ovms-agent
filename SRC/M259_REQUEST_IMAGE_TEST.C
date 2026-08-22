#include <stdio.h>
#include <string.h>

#include "llm_internal.h"
#include "LLM_REQUEST_BASIC.H"

static void remove_all(const char *path)
{
    while (remove(path) == 0) { }
}

static int write_binary(const char *path,
                        const unsigned char *data,
                        size_t size)
{
    FILE *file;

    file = fopen(path, "wb");
    if (file == NULL) return 0;
    if (fwrite(data, 1U, size, file) != size) {
        (void)fclose(file);
        return 0;
    }
    return fclose(file) == 0;
}

static int read_request(char *output, size_t output_size)
{
    FILE *file;
    size_t used;
    int ch;

    file = fopen(OPENAI_REQUEST_FILE, "r");
    if (file == NULL || output == NULL || output_size == 0U) return 0;
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
    static const unsigned char png[] = {
        0x89U, 0x50U, 0x4eU, 0x47U,
        0x0dU, 0x0aU, 0x1aU, 0x0aU
    };
    char request[8192];

    remove_all("M259_REQ.PNG");
    remove_all(OPENAI_REQUEST_FILE);
    if (!write_binary("M259_REQ.PNG", png, sizeof(png))) return 2;

    if (!write_request_image("m259-test-model",
                             "Explain this image",
                             "resp_previous",
                             "M259_REQ.PNG") ||
        !read_request(request, sizeof(request))) {
        remove_all("M259_REQ.PNG");
        remove_all(OPENAI_REQUEST_FILE);
        (void)puts("M259 image request failed: serialization.");
        return 2;
    }

    if (strstr(request, "\"model\":\"m259-test-model\"") == NULL ||
        strstr(request, "\"role\":\"user\"") == NULL ||
        strstr(request, "\"type\":\"input_text\"") == NULL ||
        strstr(request, "\"text\":\"Explain this image\"") == NULL ||
        strstr(request, "\"type\":\"input_image\"") == NULL ||
        strstr(request, "\"image_url\":\"data:image/png;base64,iVBORw0KGgo=\"") == NULL ||
        strstr(request, "\"previous_response_id\":\"resp_previous\"") == NULL ||
        strstr(request, "\"parallel_tool_calls\":false") == NULL) {
        remove_all("M259_REQ.PNG");
        remove_all(OPENAI_REQUEST_FILE);
        (void)puts("M259 image request failed: unexpected JSON shape.");
        return 2;
    }

    remove_all(OPENAI_REQUEST_FILE);
    if (write_request_image("m259-test-model",
                            "Explain this image",
                            NULL,
                            "../M259_REQ.PNG") ||
        fopen(OPENAI_REQUEST_FILE, "r") != NULL) {
        remove_all("M259_REQ.PNG");
        remove_all(OPENAI_REQUEST_FILE);
        (void)puts("M259 image request failed: unsafe path handling.");
        return 2;
    }

    remove_all("M259_REQ.PNG");
    remove_all(OPENAI_REQUEST_FILE);
    (void)puts("M259 multimodal request serialization regression passed.");
    return 1;
}
