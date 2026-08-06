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

int agent_initialize(agent_state *state)
{
    const char *api_key;
    const char *dcl_enabled;

    if (state == NULL) {
        return 0;
    }

    state->running = 1;
    state->project_root = getenv("OVMS_AGENT_ROOT");
    api_key = getenv("OPENAI_API_KEY");
    dcl_enabled = getenv("OVMS_AGENT_DCL_ENABLED");

    state->api_key_defined =
        api_key != NULL && *api_key != '\0';
    state->write_enabled = 0;
    state->dcl_enabled = agent_value_is_true(dcl_enabled);

    if (state->project_root != NULL &&
        *state->project_root != '\0') {
        if (chdir(state->project_root) != 0) {
            (void)fprintf(stderr,
                "Unable to set project root as current directory.\n");
            return 0;
        }
    }

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
