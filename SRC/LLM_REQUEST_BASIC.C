#include <stdio.h>
#include <stdlib.h>

#include "llm_internal.h"
#include "LLM_IMAGE.H"
#include "LLM_REQUEST_BASIC.H"
#include "LLM_LANGUAGE.H"
#include "LLM_REQUEST_LIMIT.INC"

static char *m288_basic_instructions(const char *prompt)
{
    char *instructions;

    instructions = llm_lang_merge("", prompt);
    if (instructions != NULL && llm_lang_last()[0] != '\0') {
        (void)printf("Language knowledge: %s (%lu bytes).\n",
                     llm_lang_last(),
                     (unsigned long)llm_lang_last_bytes());
    }
    return instructions;
}

int write_request(const char *model,
                  const char *prompt,
                  const char *previous_id)
{
    FILE *file;
    char *language_instructions;
    int success;

    language_instructions = m288_basic_instructions(prompt);
    if (language_instructions == NULL) {
        return 0;
    }

    file = fopen(LLM_REQUEST_FILE, "w");

    if (file == NULL) {
        (void)printf("Unable to create %s: %s\n",
                     LLM_REQUEST_FILE,
                     strerror(errno));
        free(language_instructions);
        return 0;
    }

    success = 1;

    if (fputs("{\"model\":\"", file) == EOF ||
        !json_write_escaped(file, model) ||
        fputc('"', file) == EOF) {
        success = 0;
    }

    if (success && llm_lang_last()[0] != '\0') {
        if (fputs(",\"instructions\":\"", file) == EOF ||
            !json_write_escaped(file, language_instructions) ||
            fputc('"', file) == EOF) {
            success = 0;
        }
    }

    if (success &&
        (fputs(",\"input\":\"", file) == EOF ||
         !json_write_escaped(file, prompt) ||
         fputc('"', file) == EOF)) {
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

    if (success && !llm_write_output_limit(file)) {
        success = 0;
    }

    if (success &&
        fputs(",\"parallel_tool_calls\":false}\n", file) == EOF) {
        success = 0;
    }

    if (fclose(file) != 0) {
        success = 0;
    }
    free(language_instructions);

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
    char *language_instructions;
    llm_image_meta meta;
    int success;

    if (model == NULL || *model == '\0' ||
        prompt == NULL || *prompt == '\0' ||
        image_path == NULL || *image_path == '\0' ||
        !llm_image_info(image_path, &meta)) {
        return 0;
    }

    language_instructions = m288_basic_instructions(prompt);
    if (language_instructions == NULL) {
        return 0;
    }

    file = fopen(LLM_REQUEST_FILE, "w");
    if (file == NULL) {
        (void)printf("Unable to create %s: %s\n",
                     LLM_REQUEST_FILE,
                     strerror(errno));
        free(language_instructions);
        return 0;
    }

    success = 1;
    if (fputs("{\"model\":\"", file) == EOF ||
        !json_write_escaped(file, model) ||
        fputc('"', file) == EOF) {
        success = 0;
    }

    if (success && llm_lang_last()[0] != '\0') {
        if (fputs(",\"instructions\":\"", file) == EOF ||
            !json_write_escaped(file, language_instructions) ||
            fputc('"', file) == EOF) {
            success = 0;
        }
    }

    if (success &&
        (fputs(",\"input\":[{\"role\":\"user\",\"content\":["
               "{\"type\":\"input_text\",\"text\":\"", file) == EOF ||
         !json_write_escaped(file, prompt) ||
         fputs("\"},{\"type\":\"input_image\",\"image_url\":\"", file) == EOF ||
         !llm_image_write_data(file, meta.path, NULL) ||
         fputs("\"}]}]", file) == EOF)) {
        success = 0;
    }

    if (success && previous_id != NULL && *previous_id != '\0') {
        if (fputs(",\"previous_response_id\":\"", file) == EOF ||
            !json_write_escaped(file, previous_id) ||
            fputc('"', file) == EOF) {
            success = 0;
        }
    }

    if (success && !llm_write_output_limit(file)) {
        success = 0;
    }

    if (success &&
        fputs(",\"parallel_tool_calls\":false}\n", file) == EOF) {
        success = 0;
    }

    if (fclose(file) != 0) success = 0;
    free(language_instructions);

    if (!success) {
        (void)remove(LLM_REQUEST_FILE);
        (void)puts("Unable to write complete image API request.");
        return 0;
    }

    return 1;
}
