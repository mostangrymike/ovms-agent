#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "llm_internal.h"
#include "LLM_JSON_PARSE.H"
#include "LLM_RESPONSE.H"
#include "LLM_TRANSPORT.H"
#include "ANSI_TERM.H"

int save_response_id(const char *json)
{
    const char *id_value;
    char *decoded;

    id_value = find_string_value(json, "id");

    if (id_value == NULL) {
        return 0;
    }

    decoded = json_decode_string(id_value, NULL);

    if (decoded == NULL) {
        return 0;
    }

    if (strlen(decoded) >= sizeof(previous_response_id)) {
        free(decoded);
        return 0;
    }

    (void)strcpy(previous_response_id, decoded);
    free(decoded);
    return 1;
}

int llm_response_empty_completed(const char *json,
                                 long *output_tokens)
{
    const char *status_value;
    const char *position;
    char *status;
    char *items;
    long tokens;
    int completed;
    int empty;

    if (output_tokens != NULL) {
        *output_tokens = 0L;
    }

    if (json == NULL) {
        return 0;
    }

    status_value = find_string_value(json, "status");
    if (status_value == NULL) {
        return 0;
    }

    status = json_decode_string(status_value, NULL);
    if (status == NULL) {
        return 0;
    }

    completed = strcmp(status, "completed") == 0;
    free(status);
    if (!completed) {
        return 0;
    }

    items = extract_output_items_json(json);
    if (items == NULL) {
        return 0;
    }

    position = skip_space(items);
    empty = *position == '\0';
    free(items);
    if (!empty) {
        return 0;
    }

    tokens = 0L;
    (void)extract_integer_argument(json, "output_tokens", &tokens);
    if (output_tokens != NULL) {
        *output_tokens = tokens;
    }

    return 1;
}

static int llm_text_known_tool(const char *name)
{
    static const char *tools[] = {
        "list_directory",
        "read_file",
        "read_file_range",
        "search_file",
        "replace_text",
        "replace_lines",
        "structured_patch",
        "create_file",
        "run_build",
        NULL
    };
    const char **tool;

    if (name == NULL || *name == '\0') {
        return 0;
    }

    for (tool = tools; *tool != NULL; ++tool) {
        if (strcmp(name, *tool) == 0) {
            return 1;
        }
    }

    return 0;
}

int llm_text_tool_like(const char *text,
                       char *tool_name,
                       size_t tool_name_size)
{
    const char *start;
    const char *end;
    const char *value;
    char *decoded;
    int bracketed;
    int has_arguments;

    if (tool_name != NULL && tool_name_size > 0U) {
        tool_name[0] = '\0';
    }

    if (text == NULL) {
        return 0;
    }

    start = text;
    while (*start == ' ' || *start == '\t' ||
           *start == '\r' || *start == '\n') {
        ++start;
    }

    end = text + strlen(text);
    while (end > start &&
           (end[-1] == ' ' || end[-1] == '\t' ||
            end[-1] == '\r' || end[-1] == '\n')) {
        --end;
    }

    if (end <= start) {
        return 0;
    }

    bracketed =
        ((*start == '{' && end[-1] == '}') ||
         (*start == '[' && end[-1] == ']'));
    if (!bracketed) {
        return 0;
    }

    has_arguments =
        strstr(start, "\"parameters\"") != NULL ||
        strstr(start, "\"arguments\"") != NULL;
    if (!has_arguments) {
        return 0;
    }

    value = find_string_value(start, "name");
    if (value == NULL) {
        return 0;
    }

    decoded = json_decode_string(value, NULL);
    if (decoded == NULL) {
        return 0;
    }

    if (!llm_text_known_tool(decoded)) {
        free(decoded);
        return 0;
    }

    if (tool_name != NULL && tool_name_size > 0U &&
        strlen(decoded) < tool_name_size) {
        (void)strcpy(tool_name, decoded);
    }

    free(decoded);
    return 1;
}

int display_clean_response(void)
{
    char *json;
    const char *text_value;
    char *decoded;
    long output_tokens;

    json = read_entire_file(LLM_RESPONSE_FILE, NULL);

    if (json == NULL) {
        return 0;
    }

    (void)save_response_id(json);

    decoded = extract_output_text_from_json(json);

    if (decoded != NULL) {
        if (!llm_transport_streamed()) {
            ansi_term_puts("");
            ansi_term_puts(decoded);
        }
        free(decoded);
        free(json);
        return 1;
    }

    if (llm_response_empty_completed(json, &output_tokens)) {
        ansi_term_puts("");
        if (output_tokens > 0L) {
            ansi_term_printf(
                "Provider completed the response with an empty output array "
                "after reporting %ld output tokens; no assistant text was "
                "returned.\n",
                output_tokens
            );
        } else {
            ansi_term_puts(
                "Provider completed the response with an empty output array; "
                "no assistant text was returned."
            );
        }
        free(json);
        return 1;
    }

    text_value = find_string_value(json, "message");

    if (text_value != NULL) {
        decoded = json_decode_string(text_value, NULL);

        if (decoded != NULL) {
            ansi_term_puts("");
            ansi_term_printf("Provider API error: %s\n", decoded);
            free(decoded);
            free(json);
            return 1;
        }
    }

    free(json);
    return 0;
}

void display_raw_response(void)
{
    FILE *file;
    char line[2048];

    file = fopen(LLM_RESPONSE_FILE, "r");

    if (file == NULL) {
        ansi_term_printf("Unable to open %s: %s\n",
                         LLM_RESPONSE_FILE,
                         strerror(errno));
        return;
    }

    while (fgets(line, sizeof(line), file) != NULL) {
        ansi_term_write(line);
    }

    (void)fclose(file);
}

int display_api_error_from_json(const char *json)
{
    const char *message_value;
    char *decoded;

    message_value = find_string_value(json, "message");

    if (message_value == NULL) {
        return 0;
    }

    decoded = json_decode_string(message_value, NULL);

    if (decoded == NULL) {
        return 0;
    }

    ansi_term_printf("Provider API error: %s\n", decoded);
    free(decoded);
    return 1;
}

int display_output_text_from_json(const char *json)
{
    char *decoded;
    char *call_name;
    char *call_id;
    char *call_args;
    char text_tool[64];
    int agent_workflow;

    decoded = extract_output_text_from_json(json);

    if (decoded == NULL) {
        return 0;
    }

    agent_workflow =
        llm_last_workflow == LLM_WORKFLOW_AGENT ||
        llm_last_workflow == LLM_WORKFLOW_WRITE ||
        llm_last_workflow == LLM_WORKFLOW_FIX;

    if (agent_workflow &&
        llm_text_tool_like(decoded, text_tool, sizeof(text_tool))) {
        if (!llm_transport_streamed()) {
            ansi_term_puts("");
            ansi_term_puts(decoded);
        }
        ansi_term_puts("");
        ansi_term_printf(
            "Provider returned tool-like assistant text for %s without a "
            "structured function call; OVMS Agent did not execute it.\n",
            text_tool[0] != '\0' ? text_tool : "an advertised tool"
        );
        free(decoded);
        return 0;
    }

    if (agent_workflow &&
        !llm_auto_final_has_evidence(llm_last_workflow)) {
        call_name = NULL;
        call_id = NULL;
        call_args = NULL;

        if (extract_function_call(
                json,
                &call_name,
                &call_id,
                &call_args)) {
            free(call_name);
            free(call_id);
            free(call_args);
            free(decoded);
            return 0;
        }

        if (!llm_transport_streamed()) {
            ansi_term_puts("");
            ansi_term_puts(decoded);
        }
        ansi_term_puts("");
        ansi_term_puts(
            "Agent returned final text before executing any project tool; "
            "treating this as an evidence-free early final."
        );
        free(decoded);
        return 0;
    }

    if (!llm_transport_streamed()) {
        ansi_term_puts("");
        ansi_term_puts(decoded);
    }
    free(decoded);
    return 1;
}
