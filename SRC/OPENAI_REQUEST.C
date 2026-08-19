#include "llm_internal.h"
#include "openai_tool_schema.h"
#include "openai_transport.h"
#include "openai_request_build.h"
#include "openai_request_agent.h"
#include "openai_request_basic.h"
#include "llm_config.h"

void openai_send(agent_state *state,
                 const char *prompt,
                 int continue_conversation)
{
    const char *api_key;
    const char *model;
    const char *previous_id;

    (void)state;

    openai_last_workflow = continue_conversation ?
        OPENAI_WORKFLOW_CHAT : OPENAI_WORKFLOW_ASK;

    openai_log_event(
        openai_workflow_name(openai_last_workflow),
        "start",
        0
    );

    if (prompt == NULL || *prompt == '\0') {
        (void)puts("Prompt cannot be empty.");
        return;
    }

    api_key = llm_api_key();
    model = llm_model();

    if (api_key == NULL || *api_key == '\0') {
        (void)puts(
            "No service access key is configured for the active AI provider."
        );
        return;
    }

    if (model == NULL || *model == '\0') {
        (void)puts("No model is configured for the active AI provider.");
        return;
    }

    previous_id = NULL;

    if (continue_conversation &&
        previous_response_id[0] != '\0') {
        previous_id = previous_response_id;
    }

    if (!write_request(model, prompt, previous_id) ||
        !write_headers(api_key)) {
        remove_temporary_files();
        return;
    }

    (void)puts("Sending request to configured AI provider...");

    if (!perform_openai_request()) {
        remove_temporary_files();
        (void)puts("AI provider request failed.");
        return;
    }

    remove_temporary_files();

    if (!display_clean_response()) {
        (void)puts("");
        (void)puts(
            "Unable to extract assistant text; raw response follows."
        );
        display_raw_response();
    }

    (void)putchar('\n');
}

