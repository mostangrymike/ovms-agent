#include <stdio.h>

#include "openai_internal.h"
#include "openai_request_agent.h"
#include "openai_tool_schema.h"

int openai_auto_partial_limit(void);

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

    if (success &&
        previous_id != NULL &&
        *previous_id != '\0') {
        if (fputs(",\"previous_response_id\":\"", file) == EOF ||
            !json_write_escaped(file, previous_id) ||
            fputc('"', file) == EOF) {
            success = 0;
        }
    }

    if (success && call_id != NULL && tool_output != NULL) {
        if (fputs(",\"input\":[{\"type\":\"function_call_output\","
                  "\"call_id\":\"", file) == EOF ||
            !json_write_escaped(file, call_id) ||
            fputs("\",\"output\":\"", file) == EOF ||
            !json_write_escaped(file, tool_output) ||
            fputs("\"}],", file) == EOF) {
            success = 0;
        }
    } else if (success) {
        if (fputs(",\"input\":\"", file) == EOF ||
            !json_write_escaped(file, user_prompt) ||
            fputs("\",", file) == EOF) {
            success = 0;
        }
    }

    if (success && !write_agent_tools_with_create(file)) {
        success = 0;
    }

    /*
     * The first response may inspect existing project files. Once context
     * exists, require the model to invoke create_file instead of asking for
     * confirmation in ordinary text.
     */
    if (success &&
        previous_id != NULL &&
        *previous_id != '\0') {
        if (fputs(
                ",\"tool_choice\":{"
                "\"type\":\"function\","
                "\"name\":\"create_file\""
                "}",
                file
            ) == EOF) {
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

    if (success &&
        previous_id != NULL &&
        *previous_id != '\0') {
        if (fputs(",\"previous_response_id\":\"", file) == EOF ||
            !json_write_escaped(file, previous_id) ||
            fputc('"', file) == EOF) {
            success = 0;
        }
    }

    if (success && call_id != NULL && tool_output != NULL) {
        if (fputs(",\"input\":[{\"type\":\"function_call_output\","
                  "\"call_id\":\"", file) == EOF ||
            !json_write_escaped(file, call_id) ||
            fputs("\",\"output\":\"", file) == EOF ||
            !json_write_escaped(file, tool_output) ||
            fputs("\"}],", file) == EOF) {
            success = 0;
        }
    } else if (success) {
        if (fputs(",\"input\":\"", file) == EOF ||
            !json_write_escaped(file, user_prompt) ||
            fputs("\",", file) == EOF) {
            success = 0;
        }
    }

    if (success) {
        if (allow_write) {
            success = write_agent_tools_with_replace(file);
        } else {
            success = write_agent_tools(file);
        }
    }

    if (success &&
        allow_write &&
        previous_id != NULL &&
        *previous_id != '\0') {
        if (fputs(",\"tool_choice\":\"required\"", file) == EOF) {
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

    if (openai_auto_partial_limit()) {
        (void)puts(
            "Incomplete guarded write reached its automatic limit; "
            "skipping final synthesis so rollback can run immediately."
        );
        return 0;
    }

    if (model == NULL ||
        previous_id == NULL || *previous_id == '\0' ||
        call_id == NULL || *call_id == '\0' ||
        tool_output == NULL) {
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
        fputs("\",\"previous_response_id\":\"", file) == EOF ||
        !json_write_escaped(file, previous_id) ||
        fputs("\",\"input\":[{\"type\":\"function_call_output\","
              "\"call_id\":\"", file) == EOF ||
        !json_write_escaped(file, call_id) ||
        fputs("\",\"output\":\"", file) == EOF ||
        !json_write_escaped(file, tool_output) ||
        fputs("\"}],\"parallel_tool_calls\":false}\n", file) == EOF) {
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

    if (success &&
        previous_id != NULL &&
        *previous_id != '\0') {
        if (fputs(",\"previous_response_id\":\"", file) == EOF ||
            !json_write_escaped(file, previous_id) ||
            fputc('"', file) == EOF) {
            success = 0;
        }
    }

    if (success && call_id != NULL && tool_output != NULL) {
        if (fputs(",\"input\":[{\"type\":\"function_call_output\","
                  "\"call_id\":\"", file) == EOF ||
            !json_write_escaped(file, call_id) ||
            fputs("\",\"output\":\"", file) == EOF ||
            !json_write_escaped(file, tool_output) ||
            fputs("\"}],", file) == EOF ||
            !write_agent_tools(file)) {
            success = 0;
        }
    } else if (success) {
        if (fputs(",\"input\":\"", file) == EOF ||
            !json_write_escaped(file, user_prompt) ||
            fputs("\",", file) == EOF ||
            !write_agent_tools(file)) {
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
        (void)puts("Unable to write complete agent request.");
        return 0;
    }

    return 1;
}
