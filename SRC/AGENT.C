#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "agent.h"
#include "LLM.H"
#include "llm_config.h"
#include "SETTINGS.H"

static int agent_value_is_true(const char *value)
{
    char normalized[8];
    size_t index;

    if (value == NULL || *value == '\0') {
        return 0;
    }

    index = 0U;

    while (value[index] != '\0' &&
           index + 1U < sizeof(normalized)) {
        normalized[index] =
            (char)toupper((unsigned char)value[index]);
        ++index;
    }

    if (value[index] != '\0') {
        return 0;
    }

    normalized[index] = '\0';

    return strcmp(normalized, "YES") == 0 ||
           strcmp(normalized, "TRUE") == 0 ||
           strcmp(normalized, "1") == 0 ||
           strcmp(normalized, "ON") == 0;
}

int agent_api_key_present(const char *api_key)
{
    return api_key != NULL && *api_key != '\0';
}

int agent_root_matches(const char *root)
{
    char current[OVMS_AGENT_ROOT_SIZE];
    char resolved[OVMS_AGENT_ROOT_SIZE];
    int matched;

    if (root == NULL || *root == '\0') {
        return 1;
    }

    if (getcwd(current, sizeof(current)) == NULL) {
        return 0;
    }

    if (chdir(root) != 0) {
        return 0;
    }

    if (getcwd(resolved, sizeof(resolved)) == NULL) {
        (void)chdir(current);
        return 0;
    }

    matched = strcmp(current, resolved) == 0;

    if (chdir(current) != 0) {
        return 0;
    }

    return matched;
}

int agent_initialize(agent_state *state)
{
    const char *api_key;
    const char *inherited_root;
    const char *approval_logical;
    const char *approval_saved;
    const llm_provider *provider;
    int quiet;

    if (state == NULL) {
        return 0;
    }

    state->running = 1;
    state->project_root = NULL;
    state->project_root_text[0] = '\0';

    inherited_root = getenv("OVMS_AGENT_ROOT");
    api_key = llm_api_key();
    quiet = agent_value_is_true(getenv("OVMS_AGENT_QUIET"));

    state->api_key_defined =
        agent_api_key_present(api_key);
    state->write_enabled = settings_effective_bool(
        "guarded_writes", "OVMS_AGENT_WRITE_ENABLED", 0);
    state->dcl_enabled = settings_effective_bool(
        "dcl_execution", "OVMS_AGENT_DCL_ENABLED", 0);

    approval_logical = getenv("OVMS_AGENT_APPROVAL_POLICY");
    if ((approval_logical == NULL || *approval_logical == '\0') &&
        settings_is_saved("approval_policy")) {
        approval_saved = settings_get("approval_policy");
        if (approval_saved != NULL && *approval_saved != '\0') {
            (void)llm_set_approval(approval_saved);
        }
    }

    if (getcwd(state->project_root_text,
               sizeof(state->project_root_text)) == NULL) {
        (void)fprintf(
            stderr,
            "Unable to determine the current project directory.\n"
        );
        return 0;
    }

    if (inherited_root != NULL &&
        *inherited_root != '\0' &&
        !agent_root_matches(inherited_root)) {
        (void)fprintf(
            stderr,
            "Refusing stale OVMS_AGENT_ROOT.\n"
        );
        (void)fprintf(
            stderr,
            "Current default: %s\n",
            state->project_root_text
        );
        (void)fprintf(
            stderr,
            "Inherited root:  %s\n",
            inherited_root
        );
        (void)fprintf(
            stderr,
            "Set default to the intended project or deassign "
            "OVMS_AGENT_ROOT.\n"
        );
        return 0;
    }

    state->project_root = state->project_root_text;

    if (!quiet) {
        (void)puts("OVMS Agent");
        (void)puts("Native agentic programming assistant for OpenVMS");
        (void)printf("Version %s\n", OVMS_AGENT_VERSION);

        provider = llm_prov_active();

        if (state->api_key_defined && provider != NULL) {
            (void)printf(
                "AI provider: %s (%s).\n\n",
                provider->name,
                provider->model
            );
        } else {
            (void)puts("AI provider: not configured.");
            (void)puts("AI-backed commands are unavailable; local commands remain usable.");
            (void)puts(
                "Use PROVIDER ADD to configure an artificial intelligence service.\n"
            );
        }
    }

    return 1;
}

void agent_shutdown(agent_state *state)
{
    if (state != NULL) {
        state->running = 0;
    }

    if (!agent_value_is_true(getenv("OVMS_AGENT_QUIET"))) {
        (void)puts("OVMS Agent terminated.");
    }
}

int agent_is_running(const agent_state *state)
{
    return state != NULL && state->running;
}

void agent_stop(agent_state *state)
{
    if (state != NULL) {
        state->running = 0;
    }
}