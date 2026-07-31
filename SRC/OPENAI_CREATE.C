#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "openai_internal.h"
#include "openai_prompts.h"

void openai_agent_create(agent_state *state, const char *goal)
{
    const char *instructions;

    const char *api_key;
    const char *model;
    char *previous_id;
    char *tool_output;
    char *call_id;
    unsigned int turn;
    int create_attempted;
    openai_file_cache_entry cache[OPENAI_AGENT_CACHE_SIZE];

    openai_last_workflow = OPENAI_WORKFLOW_CREATE;
    openai_log_event("AGENT/CREATE", "start", 0);

    if (state == NULL ||
        state->project_root == NULL ||
        *state->project_root == '\0') {
        (void)puts("OVMS_AGENT_ROOT is not defined.");
        return;
    }

    if (goal == NULL || *goal == '\0') {
        (void)puts("Usage: AGENT/CREATE goal");
        return;
    }

    if (openai_path_is_sensitive(goal)) {
        (void)puts(
            "Request denied because it references a sensitive path."
        );
        return;
    }

    api_key = getenv("OPENAI_API_KEY");
    model = getenv("OVMS_AGENT_MODEL");

    if (api_key == NULL || *api_key == '\0') {
        (void)puts("OPENAI_API_KEY is not defined.");
        return;
    }

    if (model == NULL || *model == '\0') {
        (void)puts("OVMS_AGENT_MODEL is not defined.");
        return;
    }

    instructions = openai_prompt_create();

    if (!write_headers(api_key)) {
        return;
    }

    previous_id = NULL;
    tool_output = NULL;
    call_id = NULL;
    create_attempted = 0;
    openai_cache_init(cache);

    (void)puts("Starting guarded file-creation agent...");

    for (turn = 0U; turn < OPENAI_AGENT_MAX_TURNS; ++turn) {
        char *json;
        char *response_id;
        char *name;
        char *new_call_id;
        char *arguments;
        const openai_tool_descriptor *descriptor;

        if (!write_create_agent_request(
                model,
                instructions,
                goal,
                previous_id,
                call_id,
                tool_output)) {
            break;
        }

        free(call_id);
        call_id = NULL;
        free(tool_output);
        tool_output = NULL;

        if (!perform_openai_request()) {
            (void)puts("OpenAI request failed.");
            break;
        }

        json = read_entire_file(OPENAI_RESPONSE_FILE, NULL);

        if (json == NULL) {
            (void)puts("Unable to read OpenAI response.");
            break;
        }

        response_id = extract_top_level_id(json);

        if (response_id == NULL) {
            if (!display_api_error_from_json(json)) {
                (void)puts(
                    "Response did not contain an id or readable API error."
                );
            }

            free(json);
            break;
        }

        free(previous_id);
        previous_id = response_id;

        if (display_output_text_from_json(json)) {
            free(json);
            remove_temporary_files();
            free(previous_id);
            openai_cache_free(cache);
            return;
        }

        name = NULL;
        new_call_id = NULL;
        arguments = NULL;

        if (!extract_function_call(
                json,
                &name,
                &new_call_id,
                &arguments)) {
            (void)puts(
                "Response contained neither output text nor a function call."
            );
            free(json);
            break;
        }

        free(json);

        descriptor = openai_tool_find(name);

        if (openai_tool_is_read(descriptor)) {
            tool_output = openai_tool_execute_read(
                descriptor,
                arguments,
                cache
            );
        } else if (descriptor != NULL &&
                   descriptor->kind == OPENAI_TOOL_CREATE_FILE) {
            char *display_path;
            openai_create_result create_result;

            if (create_attempted) {
                (void)puts(
                    "Second create request rejected; only one new file "
                    "is allowed per AGENT/CREATE run."
                );
                free(name);
                free(arguments);
                free(new_call_id);
                break;
            }

            create_attempted = 1;
            display_path = NULL;
            create_result = execute_create_file_tool(
                arguments,
                &display_path
            );

            (void)printf(
                "Tool requested: create_file %s\n",
                display_path != NULL ? display_path : ""
            );

            free(display_path);
            free(name);
            free(arguments);
            free(new_call_id);

            if (create_result == OPENAI_CREATE_CREATED) {
                openai_log_event(
                    "AGENT/CREATE",
                    "file_created",
                    1
                );
                (void)puts("File created. Agent complete.");
            } else if (create_result == OPENAI_CREATE_DECLINED) {
                openai_log_event(
                    "AGENT/CREATE",
                    "file_create_declined",
                    0
                );
                (void)puts("File creation cancelled. Agent complete.");
            } else {
                openai_log_event(
                    "AGENT/CREATE",
                    "file_create_failed",
                    0
                );
                (void)puts("File creation failed. Agent complete.");
            }

            remove_temporary_files();
            free(previous_id);
            free(call_id);
            free(tool_output);
            openai_cache_free(cache);
            return;
        } else {
            tool_output = make_tool_error(
                "Unsupported tool requested",
                name
            );
            (void)printf(
                "Unsupported tool requested: %s\n",
                name
            );
        }

        free(name);
        free(arguments);

        if (tool_output == NULL) {
            free(new_call_id);
            (void)puts("Unable to produce tool output.");
            break;
        }

        call_id = new_call_id;
    }

    (void)puts(
        "Create agent stopped before producing a final result "
        "(tool-turn limit or error)."
    );

    remove_temporary_files();
    free(previous_id);
    free(call_id);
    free(tool_output);
    openai_cache_free(cache);
}
