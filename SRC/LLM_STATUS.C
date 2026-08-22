#include <stdlib.h>
#include <string.h>

#include "llm_config.h"

static char *m262_provider_env(const char *name)
{
    const char *value;

    if (name != NULL && strcmp(name, "OPENAI_API_KEY") == 0) {
        value = llm_api_key();
        return (char *)value;
    }

    if (name != NULL && strcmp(name, "OVMS_AGENT_MODEL") == 0) {
        value = llm_model();
        return (char *)value;
    }

    return getenv(name);
}

#define getenv m262_provider_env
#include "LLM_STATUS_CORE.INC"
#undef getenv
