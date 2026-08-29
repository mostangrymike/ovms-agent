#include <stdio.h>
#include <stdlib.h>

#include "llm_internal.h"
#include "LLM_IMAGE.H"
#include "LLM_TOOL_SCHEMA.H"
#include "LLM_LANGUAGE.H"
#include "LLM_REQUEST_LIMIT.INC"

int write_agent_image_req(const char *model,
                          const char *instructions,
                          const char *user_prompt,
                          const char *image_path,
                          int allow_write)
{
    FILE *file;
    char *effective_instructions;
    int success;

    if (model == NULL || instructions == NULL || user_prompt == NULL ||
        image_path == NULL || *image_path == '\0') return 0;

    effective_instructions = llm_lang_merge(instructions, user_prompt);
    if (effective_instructions == NULL) return 0;

    if (llm_lang_last()[0] != '\0') {
        (void)printf("Language knowledge: %s (%lu bytes).\n",
                     llm_lang_last(),
                     (unsigned long)llm_lang_last_bytes());
    }

    file = fopen(LLM_REQUEST_FILE, "w");
    if (file == NULL) {
        free(effective_instructions);
        return 0;
    }

    success = 1;
    if (fputs("{\"model\":\"", file) == EOF ||
        !json_write_escaped(file, model) ||
        fputs("\",\"instructions\":\"", file) == EOF ||
        !json_write_escaped(file, effective_instructions) ||
        fputs("\",\"input\":[{\"role\":\"user\",\"content\":["
              "{\"type\":\"input_text\",\"text\":\"", file) == EOF ||
        !json_write_escaped(file, user_prompt) ||
        fputs("\"},{\"type\":\"input_image\",\"image_url\":\"", file) == EOF ||
        !llm_image_write_data(file, image_path, NULL) ||
        fputs("\"}]}],", file) == EOF) {
        success = 0;
    }

    if (success) {
        success = allow_write ?
            write_agent_tools_with_replace(file) :
            write_agent_tools(file);
    }

    if (success && !llm_write_output_limit(file)) {
        success = 0;
    }

    if (success &&
        fputs(",\"parallel_tool_calls\":false}\n", file) == EOF) {
        success = 0;
    }

    if (fclose(file) != 0) success = 0;
    free(effective_instructions);
    if (!success) {
        (void)remove(LLM_REQUEST_FILE);
        return 0;
    }
    return 1;
}
