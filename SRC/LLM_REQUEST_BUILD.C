#include <stdio.h>

#include "llm_internal.h"
#include "LLM_REQUEST_BUILD.H"
#include "LLM_TOOL_SCHEMA.H"

int write_build_initial_request(
    const char *model,
    const char *instructions,
    const char *user_prompt)
{
    FILE *file;
    int success;

    file = fopen(LLM_REQUEST_FILE, "w");

    if (file == NULL) {
        (void)printf("Unable to create %s: %s\n",
                     LLM_REQUEST_FILE,
                     strerror(errno));
        return 0;
    }

    success = 1;

    if (fputs("{\"model\":\"", file) == EOF ||
        !json_write_escaped(file, model) ||
        fputs("\",\"instructions\":\"", file) == EOF ||
        !json_write_escaped(file, instructions) ||
        fputs("\",\"input\":\"", file) == EOF ||
        !json_write_escaped(file, user_prompt) ||
        fputs("\",", file) == EOF ||
        !write_build_agent_tools(file) ||
        fputs(",\"tool_choice\":{" 
              "\"type\":\"function\","
              "\"name\":\"run_build\""
              "},\"store\":true,"
              "\"parallel_tool_calls\":false}\n", file) == EOF) {
        success = 0;
    }

    if (fclose(file) != 0) {
        success = 0;
    }

    if (!success) {
        (void)puts("Unable to write initial build-agent request.");
        return 0;
    }

    return 1;
}

int write_build_followup_request(
    const char *model,
    const char *instructions,
    const char *context_items,
    const char *call_id,
    const char *tool_output)
{
    FILE *file;
    int success;

    file = fopen(LLM_REQUEST_FILE, "w");

    if (file == NULL) {
        (void)printf("Unable to create %s: %s\n",
                     LLM_REQUEST_FILE,
                     strerror(errno));
        return 0;
    }

    success = 1;

    if (fputs("{\"model\":\"", file) == EOF ||
        !json_write_escaped(file, model) ||
        fputs("\",\"instructions\":\"", file) == EOF ||
        !json_write_escaped(file, instructions) ||
        fputs("\",\"input\":[", file) == EOF ||
        fputs(context_items, file) == EOF ||
        fputs(",{\"type\":\"function_call_output\","
              "\"call_id\":\"", file) == EOF ||
        !json_write_escaped(file, call_id) ||
        fputs("\",\"output\":\"", file) == EOF ||
        !json_write_escaped(file, tool_output) ||
        fputs("\"}],", file) == EOF ||
        !write_build_agent_tools(file) ||
        fputs(",\"store\":true,"
              "\"parallel_tool_calls\":false}\n", file) == EOF) {
        success = 0;
    }

    if (fclose(file) != 0) {
        success = 0;
    }

    if (!success) {
        (void)puts("Unable to write build-agent follow-up request.");
        return 0;
    }

    return 1;
}
