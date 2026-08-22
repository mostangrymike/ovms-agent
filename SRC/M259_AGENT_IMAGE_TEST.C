#include <stdio.h>
#include <string.h>

#include "command_internal.h"
#include "llm_internal.h"

/* Standalone regression stubs for command-layer helpers pulled in through
   LLM_PLAN.OBJ. The agent-image test does not exercise interactive input. */
int command_line_complete(const char *input, size_t length, int eof)
{
    (void)input;
    (void)length;
    (void)eof;
    return 0;
}

int command_read_stream(FILE *stream, char *buffer, size_t buffer_size)
{
    (void)stream;
    (void)buffer;
    (void)buffer_size;
    return 0;
}

int llm_test_agent_image_req(const char *model,
                             const char *instructions,
                             const char *goal,
                             const char *image_path);
int llm_image_log(const char *image_path,
                  const char *outcome,
                  int status);
void llm_test_set_log_path(const char *path);

static void remove_all(const char *path)
{
    while (remove(path) == 0) { }
}

static int write_png(const char *path)
{
    static const unsigned char data[12] = {
        0x89U, 0x50U, 0x4eU, 0x47U, 0x0dU, 0x0aU,
        0x1aU, 0x0aU, 0x00U, 0x00U, 0x00U, 0x00U
    };
    FILE *file;

    file = fopen(path, "wb");
    if (file == NULL) return 0;
    if (fwrite(data, 1U, sizeof(data), file) != sizeof(data)) {
        (void)fclose(file);
        return 0;
    }
    return fclose(file) == 0;
}

static int read_text(const char *path, char *output, size_t output_size)
{
    FILE *file;
    size_t used;
    int ch;

    if (output == NULL || output_size == 0U) return 0;
    file = fopen(path, "r");
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

static int require_true(int condition, const char *message)
{
    if (!condition) {
        (void)fprintf(stderr, "M259 agent-image regression failed: %s\n", message);
        return 0;
    }
    return 1;
}

int main(void)
{
    static const char image_path[] = "M259_AGENT_IMAGE.PNG";
    static const char log_path[] = "M259_IMAGE_ACTIVITY.LOG";
    char request[16384];
    char log_text[4096];
    const command_entry *entry;

    remove_all(image_path);
    remove_all(log_path);
    remove_all(LLM_REQUEST_FILE);

    if (!write_png(image_path)) {
        (void)puts("M259 agent-image regression failed: fixture setup.");
        return 2;
    }

    command_registry_initialize();
    entry = command_find("AGENT/IMAGE");
    if (!require_true(entry != NULL && entry->handler != NULL,
                      "AGENT/IMAGE command registration")) {
        remove_all(image_path);
        return 2;
    }

    if (!require_true(
            llm_test_agent_image_req(
                "m259-agent-model",
                "Read-only agent instructions",
                "Inspect the screenshot and identify the relevant source area.",
                image_path),
            "agent image request serialization") ||
        !read_text(LLM_REQUEST_FILE, request, sizeof(request)) ||
        !require_true(
            strstr(request, "\"type\":\"input_image\"") != NULL &&
            strstr(request, "data:image/png;base64,") != NULL &&
            strstr(request, "\"type\":\"input_text\"") != NULL &&
            strstr(request, "read_file") != NULL,
            "image request retains read-only agent tools")) {
        remove_all(image_path);
        remove_all(LLM_REQUEST_FILE);
        return 2;
    }

    llm_test_set_log_path(log_path);
    if (!require_true(llm_image_log(image_path, "image_accepted", 1),
                      "metadata log write") ||
        !read_text(log_path, log_text, sizeof(log_text)) ||
        !require_true(
            strstr(log_text, "workflow=AGENT/IMAGE") != NULL &&
            strstr(log_text, "image_accepted") != NULL &&
            strstr(log_text, "path=M259_AGENT_IMAGE.PNG") != NULL &&
            strstr(log_text, "type=image/png") != NULL &&
            strstr(log_text, "size=12") != NULL &&
            strstr(log_text, "base64") == NULL &&
            strstr(log_text, "data:image") == NULL,
            "metadata-only activity logging")) {
        llm_test_set_log_path(NULL);
        remove_all(image_path);
        remove_all(log_path);
        remove_all(LLM_REQUEST_FILE);
        return 2;
    }

    llm_test_set_log_path(NULL);
    remove_all(image_path);
    remove_all(log_path);
    remove_all(LLM_REQUEST_FILE);
    (void)puts("M259 agent image command/logging regression passed.");
    return 1;
}