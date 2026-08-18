#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "openai_internal.h"

void openai_agent_build(agent_state *state, const char *goal)
{
    static const char instructions[] =
        "You are a careful OpenVMS build analyst. Use the fixed run_build "
        "tool exactly once. After receiving the build output, summarize "
        "whether the build succeeded. If it failed, identify the first "
        "actionable compiler or linker diagnostic and recommend the smallest "
        "next edit. Do not request arbitrary commands and do not claim success "
        "unless the returned OpenVMS status is odd.";

    const char *api_key;
    const char *model;
    char *json;
    char *name;
    char *call_id;
    char *arguments;
    char *context_items;
    char *tool_output;
    const openai_tool_descriptor *descriptor;
    int build_status;

    openai_last_workflow = OPENAI_WORKFLOW_BUILD;
    openai_log_event(
        openai_workflow_name(openai_last_workflow),
        "start",
        0
    );
    openai_last_rollback = OPENAI_ROLLBACK_NONE;

    if (state == NULL ||
        state->project_root == NULL ||
        *state->project_root == '\0') {
        (void)puts("OVMS_AGENT_ROOT is not defined.");
        return;
    }

    if (goal == NULL || *goal == '\0') {
        (void)puts("Usage: AGENT/BUILD goal");
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

    if (!write_headers(api_key)) {
        return;
    }

    (void)puts("Starting controlled build agent...");

    if (!write_build_initial_request(model,
                                           instructions,
                                           goal)) {
        remove_temporary_files();
        return;
    }

    if (!perform_openai_request()) {
        (void)puts("Initial OpenAI request failed.");
        remove_temporary_files();
        return;
    }

    json = read_entire_file(OPENAI_RESPONSE_FILE, NULL);
    if (json == NULL) {
        (void)puts("Unable to read initial OpenAI response.");
        remove_temporary_files();
        return;
    }

    if (display_api_error_from_json(json)) {
        free(json);
        remove_temporary_files();
        return;
    }

    name = NULL;
    call_id = NULL;
    arguments = NULL;

    if (!extract_function_call(json, &name, &call_id, &arguments)) {
        (void)puts("Initial response did not contain a function call.");
        free(json);
        remove_temporary_files();
        return;
    }

    descriptor = openai_tool_find(name);

    if (descriptor == NULL ||
        descriptor->kind != OPENAI_TOOL_RUN_BUILD) {
        (void)printf("Unsupported build tool requested: %s\n", name);
        free(name);
        free(call_id);
        free(arguments);
        free(json);
        remove_temporary_files();
        return;
    }

    context_items = extract_output_items_json(json);
    free(name);
    free(arguments);
    free(json);

    if (context_items == NULL) {
        (void)puts("Unable to preserve initial response output items.");
        free(call_id);
        remove_temporary_files();
        return;
    }

    tool_output = execute_run_build_tool(&build_status);
    if (tool_output == NULL) {
        (void)puts("Unable to capture build output.");
        free(context_items);
        free(call_id);
        remove_temporary_files();
        return;
    }

    (void)printf("Tool executed: run_build [%s, status %d]\n",
                 (build_status & 1) != 0 ? "success" : "failure",
                 build_status);

    if (!write_build_followup_request(model,
                                            instructions,
                                            context_items,
                                            call_id,
                                            tool_output)) {
        free(tool_output);
        free(context_items);
        free(call_id);
        remove_temporary_files();
        return;
    }

    free(tool_output);
    free(context_items);
    free(call_id);

    if (!perform_openai_request()) {
        (void)puts("Follow-up OpenAI request failed.");
        remove_temporary_files();
        return;
    }

    json = read_entire_file(OPENAI_RESPONSE_FILE, NULL);
    if (json == NULL) {
        (void)puts("Unable to read follow-up response.");
        remove_temporary_files();
        return;
    }

    if (!display_output_text_from_json(json)) {
        if (!display_api_error_from_json(json)) {
            (void)puts("Follow-up response contained no readable output.");
            display_raw_response();
        }
    }

    free(json);
    remove_temporary_files();
    (void)putchar('\n');
}
