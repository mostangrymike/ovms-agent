#include <stdio.h>
#include <stdlib.h>

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

int settings_effective_bool(const char *key,
                            const char *logical_name,
                            int fallback)
{
    (void)key;
    (void)logical_name;
    return fallback;
}

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
