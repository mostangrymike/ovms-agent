#include <stdio.h>
#include <stdlib.h>

#include "llm_internal.h"
#include "LLM_REQUEST_AGENT.H"
#include "LLM_REQUEST_BUILD.H"
#include "LLM_TOOL_SCHEMA.H"
#include "LLM_LANGUAGE.H"
#include "LLM_REQUEST_LIMIT.INC"

static int m279_build_model_check(const char *model)
{
    if (llm_m279_tool_model_ok(llm_api_url(), model)) {
        return 1;
    }

    (void)puts(
        "Groq GPT-OSS agent tool workflows are not supported through the "
        "Responses transport. Use a Groq model with working Responses tool "
        "calls or another provider."
    );
    return 0;
}

int write_build_initial_request(
    const char *model,
    const char *instructions,
    const char *user_prompt)
{
    FILE *file;
    char *effective_instructions;
    int success;

    if (!m279_build_model_check(model)) {
        return 0;
    }

    effective_instructions = llm_lang_merge(instructions, user_prompt);
    if (effective_instructions == NULL) {
        return 0;
    }

    if (llm_lang_last()[0] != '\0') {
        (void)printf("Language knowledge: %s (%lu bytes).\n",
                     llm_lang_last(),
                     (unsigned long)llm_lang_last_bytes());
    }

    file = fopen(LLM_REQUEST_FILE, "w");

    if (file == NULL) {
        (void)printf("Unable to create %s: %s\n",
                     LLM_REQUEST_FILE,
                     strerror(errno));
        free(effective_instructions);
        return 0;
    }

    success = 1;

    if (fputs("{\"model\":\"", file) == EOF ||
        !json_write_escaped(file, model) ||
        fputs("\",\"instructions\":\"", file) == EOF ||
        !json_write_escaped(file, effective_instructions) ||
        fputs("\",\"input\":\"", file) == EOF ||
        !json_write_escaped(file, user_prompt) ||
        fputs("\",", file) == EOF ||
        !write_build_agent_tools(file) ||
        fputs(",\"tool_choice\":{" 
              "\"type\":\"function\","
              "\"name\":\"run_build\""
              "},\"store\":false", file) == EOF) {
        success = 0;
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
    free(effective_instructions);

    if (!success) {
        (void)puts("Unable to write initial build-agent request.");
        return 0;
    }

    return 1;
}

int write_build_followup_request(
    const char *model,
    const char *instructions,
    const char *user_prompt,
    const char *context_items,
    const char *call_id,
    const char *tool_output)
{
    FILE *file;
    char *effective_instructions;
    int success;

    if (!m279_build_model_check(model)) {
        return 0;
    }

    effective_instructions = llm_lang_merge(instructions, user_prompt);
    if (effective_instructions == NULL) {
        return 0;
    }

    file = fopen(LLM_REQUEST_FILE, "w");

    if (file == NULL) {
        (void)printf("Unable to create %s: %s\n",
                     LLM_REQUEST_FILE,
                     strerror(errno));
        free(effective_instructions);
        return 0;
    }

    success = 1;

    if (fputs("{\"model\":\"", file) == EOF ||
        !json_write_escaped(file, model) ||
        fputs("\",\"instructions\":\"", file) == EOF ||
        !json_write_escaped(file, effective_instructions) ||
        fputs("\",\"input\":[", file) == EOF ||
        fputs(context_items, file) == EOF ||
        fputs(",{\"type\":\"function_call_output\","
              "\"call_id\":\"", file) == EOF ||
        !json_write_escaped(file, call_id) ||
        fputs("\",\"output\":\"", file) == EOF ||
        !json_write_escaped(file, tool_output) ||
        fputs("\"}],", file) == EOF ||
        !write_build_agent_tools(file) ||
        fputs(",\"store\":false", file) == EOF) {
        success = 0;
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
    free(effective_instructions);

    if (!success) {
        (void)puts("Unable to write build-agent follow-up request.");
        return 0;
    }

    return 1;
}
