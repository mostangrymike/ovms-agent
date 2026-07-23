#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "agent.h"

int agent_initialize(agent_state *state)
{
    const char *api_key;

    if (state == NULL) {
        return 0;
    }

    state->running = 1;
    state->project_root = getenv("OVMS_AGENT_ROOT");
    api_key = getenv("OPENAI_API_KEY");

    state->api_key_defined =
        api_key != NULL && *api_key != '\0';
    state->write_enabled = 0;
    state->dcl_enabled = 0;

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
