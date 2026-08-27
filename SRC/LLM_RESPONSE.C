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

int display_clean_response(void)
{
    char *json;
    const char *text_value;
    char *decoded;

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
