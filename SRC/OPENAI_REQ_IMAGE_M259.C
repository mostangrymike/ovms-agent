#include <stdio.h>

#include "openai_internal.h"
#include "openai_image.h"
#include "openai_tool_schema.h"

int write_agent_image_req(const char *model,
                          const char *instructions,
                          const char *user_prompt,
                          const char *image_path,
                          int allow_write)
{
    FILE *file;
    int success;

    if (model == NULL || instructions == NULL || user_prompt == NULL ||
        image_path == NULL || *image_path == '\0') return 0;

    file = fopen(OPENAI_REQUEST_FILE, "w");
    if (file == NULL) return 0;

    success = 1;
    if (fputs("{\"model\":\"", file) == EOF ||
        !json_write_escaped(file, model) ||
        fputs("\",\"instructions\":\"", file) == EOF ||
        !json_write_escaped(file, instructions) ||
        fputs("\",\"input\":[{\"role\":\"user\",\"content\":["
              "{\"type\":\"input_text\",\"text\":\"", file) == EOF ||
        !json_write_escaped(file, user_prompt) ||
        fputs("\"},{\"type\":\"input_image\",\"image_url\":\"", file) == EOF ||
        !openai_image_write_data(file, image_path, NULL) ||
        fputs("\"}]}],", file) == EOF) {
        success = 0;
    }

    if (success) {
        success = allow_write ?
            write_agent_tools_with_replace(file) :
            write_agent_tools(file);
    }

    if (success &&
        fputs(",\"parallel_tool_calls\":false}\n", file) == EOF) {
        success = 0;
    }

    if (fclose(file) != 0) success = 0;
    if (!success) {
        (void)remove(OPENAI_REQUEST_FILE);
        return 0;
    }
    return 1;
}
