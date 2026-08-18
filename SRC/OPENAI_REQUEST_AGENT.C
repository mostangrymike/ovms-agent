#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "openai_internal.h"
#include "openai_request_agent.h"
#include "openai_tool_schema.h"
#include "OPENAI_AGENT_CTX.H"
#include "OPENAI_JSON_PARSE.H"

#define OPENAI_AGENT_GOAL_MAX 8192U

static char openai_agent_goal[OPENAI_AGENT_GOAL_MAX];

int openai_auto_partial_limit(void);

static int openai_update_local_ctx(const char *call_id,
                                   const char *tool_output)
{
    char *json;
    char *items;

    if (call_id == NULL || tool_output == NULL) {
        return 0;
    }

    json = read_entire_file(OPENAI_RESPONSE_FILE, NULL);
    if (json == NULL) {
        return 0;
    }

    items = extract_output_items_json(json);
    free(json);
    if (items == NULL) {
        return 0;
    }

    if (!openai_agent_ctx_add_items(items)) {
        free(items);
        return 0;
    }
    free(items);

    return openai_agent_ctx_add_tool(call_id, tool_output);
}

static int openai_write_local_input(FILE *file,
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
        if (goal_length >= sizeof(openai_agent_goal)) {
            return 0;
        }

        openai_agent_ctx_reset();
        (void)strcpy(openai_agent_goal, user_prompt);
    } else if (!openai_update_local_ctx(call_id, tool_output)) {
        return 0;
    }

    if (fputs(",\"input\":", file) == EOF ||
        !openai_agent_ctx_write(file, user_prompt) ||
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
    int success;

    (void)previous_id;

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
        fputs("\",\"instructions\":\"", file) == EOF ||
        !json_write_escaped(file, instructions) ||
        fputc('"', file) == EOF) {
        success = 0;
    }

    if (success) {
        success = openai_write_local_input(
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

    if (success &&
        fputs(",\"parallel_tool_calls\":false}\n", file) == EOF) {
        success = 0;
    }

    if (fclose(file) != 0) {
        success = 0;
    }

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
    int success;

    (void)previous_id;

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
        fputs("\",\"instructions\":\"", file) == EOF ||
        !json_write_escaped(file, instructions) ||
        fputc('"', file) == EOF) {
        success = 0;
    }

    if (success) {
        success = openai_write_local_input(
            file, user_prompt, call_id, tool_output);
    }

    if (success) {
        if (allow_write) {
            success = write_agent_tools_with_replace(file);
        } else {
            success = write_agent_tools(file);
        }
    }

    if (success && allow_write && call_id != NULL && tool_output != NULL) {
        if (fputs(",\"tool_choice\":\"required\"", file) == EOF) {
            success = 0;
        }
    }

    if (success &&
        fputs(",\"parallel_tool_calls\":false}\n", file) == EOF) {
        success = 0;
    }

    if (fclose(file) != 0) {
        success = 0;
    }

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
    int success;

    (void)previous_id;

    if (openai_auto_partial_limit()) {
        (void)puts(
            "Incomplete guarded write reached its automatic limit; "
            "skipping final synthesis so rollback can run immediately."
        );
        return 0;
    }

    if (model == NULL ||
        call_id == NULL || *call_id == '\0' ||
        tool_output == NULL ||
        openai_agent_goal[0] == '\0') {
        return 0;
    }

    if (!openai_update_local_ctx(call_id, tool_output)) {
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
        fputs("\",\"instructions\":\"", file) == EOF ||
        !json_write_escaped(file, instructions) ||
        fputs("\",\"input\":", file) == EOF ||
        !openai_agent_ctx_write_final(file, openai_agent_goal) ||
        fputs(",\"parallel_tool_calls\":false}\n", file) == EOF) {
        success = 0;
    }

    if (fclose(file) != 0) {
        success = 0;
    }

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
    int success;

    (void)previous_id;

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
        fputs("\",\"instructions\":\"", file) == EOF ||
        !json_write_escaped(file, instructions) ||
        fputc('"', file) == EOF) {
        success = 0;
    }

    if (success) {
        success = openai_write_local_input(
            file, user_prompt, call_id, tool_output);
    }

    if (success && !write_agent_tools(file)) {
        success = 0;
    }

    if (success &&
        fputs(",\"parallel_tool_calls\":false}\n", file) == EOF) {
        success = 0;
    }

    if (fclose(file) != 0) {
        success = 0;
    }

    if (!success) {
        (void)puts("Unable to write complete agent request.");
        return 0;
    }

    return 1;
}
