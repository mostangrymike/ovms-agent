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
#define openai_tool_descriptor llm_tool_descriptor
#define openai_tool_find llm_tool_find
#define openai_tool_is_read llm_tool_is_read
#define openai_tool_execute_read llm_tool_execute_read
#define OPENAI_TOOL_CREATE_FILE LLM_TOOL_CREATE_FILE
#include "LLM_CREATE_CORE.INC"
#undef OPENAI_TOOL_CREATE_FILE
#undef openai_tool_execute_read
#undef openai_tool_is_read
#undef openai_tool_find
#undef openai_tool_descriptor
#undef getenv
