#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "agent.h"

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
    const char *dcl_enabled;
    const char *write_enabled;
    const char *inherited_root;

    if (state == NULL) {
        return 0;
    }

    state->running = 1;
    state->project_root = NULL;
    state->project_root_text[0] = '\0';

    inherited_root = getenv("OVMS_AGENT_ROOT");
    api_key = getenv("OPENAI_API_KEY");
    dcl_enabled = getenv("OVMS_AGENT_DCL_ENABLED");
    write_enabled = getenv("OVMS_AGENT_WRITE_ENABLED");

    state->api_key_defined =
        api_key != NULL && *api_key != '\0';
    state->write_enabled = agent_value_is_true(write_enabled);
    state->dcl_enabled = agent_value_is_true(dcl_enabled);

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

    (void)puts("OVMS Agent");
    (void)puts("Native agentic programming assistant for OpenVMS");
    (void)printf("Version %s\n\n", OVMS_AGENT_VERSION);

    return 1;
}

void agent_shutdown(agent_state *state)
{
    if (state != NULL) {
        state->running = 0;
    }

    (void)puts("OVMS Agent terminated.");
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
