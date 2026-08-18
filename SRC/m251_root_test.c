#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "agent.h"
#include "llm_config.h"

const llm_provider *llm_prov_active(void)
{
    return NULL;
}

const char *llm_api_key(void)
{
    return NULL;
}

int main(void)
{
    char current[OVMS_AGENT_ROOT_SIZE];
    agent_state state;

    if (getcwd(current, sizeof(current)) == NULL) {
        (void)puts("M251 root test failed: getcwd failed.");
        return EXIT_FAILURE;
    }

    if (!agent_root_matches(current)) {
        (void)puts(
            "M251 root test failed: current directory did not match itself."
        );
        return EXIT_FAILURE;
    }

    if (agent_root_matches("M251_ROOT_DOES_NOT_EXIST")) {
        (void)puts(
            "M251 root test failed: missing inherited root was accepted."
        );
        return EXIT_FAILURE;
    }

    if (getcwd(state.project_root_text,
               sizeof(state.project_root_text)) == NULL) {
        (void)puts("M251 root test failed: state root capture failed.");
        return EXIT_FAILURE;
    }

    state.project_root = state.project_root_text;

    if (strcmp(state.project_root, current) != 0) {
        (void)puts(
            "M251 root test failed: current directory was not selected."
        );
        return EXIT_FAILURE;
    }

    (void)puts("M251 root resolution test passed.");
    return EXIT_SUCCESS;
}
