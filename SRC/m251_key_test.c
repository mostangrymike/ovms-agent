#include <stdio.h>
#include <stdlib.h>

#include "agent.h"

int main(void)
{
    if (agent_api_key_present(NULL)) {
        (void)puts("M251.6 failed: NULL API key reported present.");
        return EXIT_FAILURE;
    }

    if (agent_api_key_present("")) {
        (void)puts("M251.6 failed: empty API key reported present.");
        return EXIT_FAILURE;
    }

    if (!agent_api_key_present("test-value")) {
        (void)puts("M251.6 failed: non-empty API key reported missing.");
        return EXIT_FAILURE;
    }

    (void)puts("M251.6 API-key readiness regression passed.");
    return EXIT_SUCCESS;
}
