#include <stdio.h>

#include "llm_internal.h"
#include "openai_image.h"
#include "openai_request_basic.h"

int write_request(const char *model,
                  const char *prompt,
                  const char *previous_id)
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
        fputc('"', file) == EOF) {
        success = 0;
    }

    if (success &&
        previous_id != NULL &&
        *previous_id != '\0') {
        if (fputs(",\"previous_response_id\":\"", file) == EOF ||
            !json_write_escaped(file, previous_id) ||
            fputc('"', file) == EOF) {
            success = 0;
        }
    }

    if (success &&
        fputs(
            ",\"parallel_tool_calls\":false"
            "}\n",
            file
        ) == EOF) {
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

int write_request_image(const char *model,
                        const char *prompt,
                        const char *previous_id,
                        const char *image_path)
{
    FILE *file;
    openai_image_meta meta;
    int success;

    if (model == NULL || *model == '\0' ||
        prompt == NULL || *prompt == '\0' ||
        image_path == NULL || *image_path == '\0' ||
        !openai_image_info(image_path, &meta)) {
        return 0;
    }

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
        fputs("\",\"input\":[{\"role\":\"user\",\"content\":["
              "{\"type\":\"input_text\",\"text\":\"", file) == EOF ||
        !json_write_escaped(file, prompt) ||
        fputs("\"},{\"type\":\"input_image\",\"image_url\":\"", file) == EOF ||
        !openai_image_write_data(file, meta.path, NULL) ||
        fputs("\"}]}]", file) == EOF) {
        success = 0;
    }

    if (success && previous_id != NULL && *previous_id != '\0') {
        if (fputs(",\"previous_response_id\":\"", file) == EOF ||
            !json_write_escaped(file, previous_id) ||
            fputc('"', file) == EOF) {
            success = 0;
        }
    }

    if (success &&
        fputs(",\"parallel_tool_calls\":false}\n", file) == EOF) {
        success = 0;
    }

    if (fclose(file) != 0) success = 0;

    if (!success) {
        (void)remove(OPENAI_REQUEST_FILE);
        (void)puts("Unable to write complete image API request.");
        return 0;
    }

    return 1;
}
