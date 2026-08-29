#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "llm_internal.h"
#include "LLM_REQUEST_AGENT.H"
#include "LLM_TOOL_SCHEMA.H"
#include "LLM_AGENT_CTX.H"
#include "LLM_JSON_PARSE.H"
#include "LLM_AUTO.H"
#include "LLM_LANGUAGE.H"
#include "LLM_REQUEST_LIMIT.INC"

#define LLM_AGENT_GOAL_MAX 32768U

static char llm_agent_goal[LLM_AGENT_GOAL_MAX];

int llm_m279_tool_model_ok(const char *url,
                           const char *model)
{
    static const char groq_host[] = "api.groq.com";
    static const char gpt_oss[] = "openai/gpt-oss-";

    if (url == NULL || model == NULL) {
        return 1;
    }

    return strstr(url, groq_host) == NULL ||
           strncmp(model, gpt_oss, sizeof(gpt_oss) - 1U) != 0;
}

static int llm_m279_tool_model_check(const char *model)
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

static char *llm_m288_instructions(const char *instructions,
                                   const char *user_prompt,
                                   int report)
{
    char *merged;

    merged = llm_lang_merge(instructions, user_prompt);
    if (merged != NULL && report && llm_lang_last()[0] != '\0') {
        (void)printf("Language knowledge: %s (%lu bytes).\n",
                     llm_lang_last(),
                     (unsigned long)llm_lang_last_bytes());
    }
    return merged;
}

static int llm_update_local_ctx(const char *call_id,
                                const char *tool_output)
{
    char *json;
    char *items;

    if (call_id == NULL || tool_output == NULL) {
        return 0;
    }

    json = read_entire_file(LLM_RESPONSE_FILE, NULL);
    if (json == NULL) {
        return 0;
    }

    items = extract_output_items_json(json);
    free(json);
    if (items == NULL) {
        return 0;
    }

    if (!llm_agent_ctx_add_items(items)) {
        free(items);
        return 0;
    }
    free(items);

    return llm_agent_ctx_add_tool(call_id, tool_output);
}

static int llm_write_local_input(FILE *file,
                                 const char *user_prompt,
                                 const char *call_id,
                                 const char *tool_output)
{
    size_t goal_length;

    if (file == NULL || user_prompt == NULL) {
        return 0;
    }

    if (call_id == NULL || tool_output == NULL) {
        goal_length = strlen(user_prompt);
        if (goal_length >= sizeof(llm_agent_goal)) {
            return 0;
        }

        llm_agent_ctx_reset();
        (void)strcpy(llm_agent_goal, user_prompt);
    } else if (!llm_update_local_ctx(call_id, tool_output)) {
        return 0;
    }

    if (fputs(",\"input\":", file) == EOF ||
        !llm_agent_ctx_write(file, user_prompt) ||
        fputc(',', file) == EOF) {
        return 0;
    }

    return 1;
}

int write_create_agent_request(
    const char *model,
    const char *instructions,
    const char *user_prompt,
    const char *previous_id,
    const char *call_id,
    const char *tool_output)
{
    FILE *file;
    char *effective_instructions;
    int success;

    (void)previous_id;

    if (!llm_m279_tool_model_check(model)) {
        return 0;
    }

    effective_instructions = llm_m288_instructions(
        instructions, user_prompt, call_id == NULL && tool_output == NULL);
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
        fputc('"', file) == EOF) {
        success = 0;
    }

    if (success) {
        success = llm_write_local_input(
            file, user_prompt, call_id, tool_output);
    }

    if (success && !write_agent_tools_with_create(file)) {
        success = 0;
    }

    if (success && call_id != NULL && tool_output != NULL) {
        if (fputs(",\"tool_choice\":{"
                  "\"type\":\"function\","
                  "\"name\":\"create_file\"}", file) == EOF) {
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
    free(effective_instructions);

    if (!success) {
        (void)puts("Unable to write complete create-agent request.");
        return 0;
    }

    return 1;
}

int write_agent_request_mode(const char *model,
                             const char *instructions,
                             const char *user_prompt,
                             const char *previous_id,
                             const char *call_id,
                             const char *tool_output,
                             int allow_write)
{
    FILE *file;
    char *effective_instructions;
    int success;

    (void)previous_id;

    if (!llm_m279_tool_model_check(model)) {
        return 0;
    }

    effective_instructions = llm_m288_instructions(
        instructions, user_prompt, call_id == NULL && tool_output == NULL);
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
        fputc('"', file) == EOF) {
        success = 0;
    }

    if (success) {
        success = llm_write_local_input(
            file, user_prompt, call_id, tool_output);
    }

    if (success) {
        if (allow_write) {
            success = write_agent_tools_with_replace(file);
        } else {
            success = write_agent_tools(file);
        }
    }

    if (success && allow_write && call_id != NULL && tool_output != NULL && strstr(tool_output, "\neffect: read\n") != NULL) {
        if (fputs(",\"tool_choice\":\"required\"", file) == EOF) {
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
    free(effective_instructions);

    if (!success) {
        (void)puts("Unable to write complete agent request.");
        return 0;
    }

    return 1;
}

int write_agent_final_request(
    const char *model,
    const char *previous_id,
    const char *call_id,
    const char *tool_output)
{
    static const char instructions[] =
        "Produce the final answer now using only the evidence already gathered. "
        "Do not request, propose, or describe additional tool calls. "
        "State any remaining uncertainty explicitly. Do not claim to have "
        "inspected or changed anything not present in the supplied evidence.";
    FILE *file;
    char *effective_instructions;
    int success;

    (void)previous_id;

    if (llm_auto_partial_limit()) {
        (void)puts(
            "Incomplete guarded write reached its automatic limit; "
            "skipping final synthesis so rollback can run immediately."
        );
        return 0;
    }

    if (model == NULL ||
        call_id == NULL || *call_id == '\0' ||
        tool_output == NULL ||
        llm_agent_goal[0] == '\0') {
        return 0;
    }

    if (!llm_update_local_ctx(call_id, tool_output)) {
        return 0;
    }

    effective_instructions = llm_m288_instructions(
        instructions, llm_agent_goal, 0);
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
        fputs("\",\"input\":", file) == EOF ||
        !llm_agent_ctx_write_final(file, llm_agent_goal)) {
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
        (void)puts("Unable to write final synthesis request.");
        return 0;
    }

    return 1;
}

int write_agent_request(const char *model,
                        const char *instructions,
                        const char *user_prompt,
                        const char *previous_id,
                        const char *call_id,
                        const char *tool_output)
{
    FILE *file;
    char *effective_instructions;
    int success;

    (void)previous_id;

    if (!llm_m279_tool_model_check(model)) {
        return 0;
    }

    effective_instructions = llm_m288_instructions(
        instructions, user_prompt, call_id == NULL && tool_output == NULL);
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
        fputc('"', file) == EOF) {
        success = 0;
    }

    if (success) {
        success = llm_write_local_input(
            file, user_prompt, call_id, tool_output);
    }

    if (success && !write_agent_tools(file)) {
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
        (void)puts("Unable to write complete agent request.");
        return 0;
    }

    return 1;
}
