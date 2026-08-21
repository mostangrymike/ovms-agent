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
#define openai_status llm_status
#define openai_selftest llm_selftest
#define openai_verify llm_verify
#define openai_tool_descriptor llm_tool_descriptor
#define openai_tool_find llm_tool_find
#define openai_tool_is_replace llm_tool_is_replace
#define OPENAI_TOOL_READ_FILE LLM_TOOL_READ_FILE
#define OPENAI_TOOL_REPLACE_TEXT LLM_TOOL_REPLACE_TEXT
#define OPENAI_TOOL_RUN_BUILD LLM_TOOL_RUN_BUILD
#include "LLM_STATUS_CORE.INC"
#undef OPENAI_TOOL_RUN_BUILD
#undef OPENAI_TOOL_REPLACE_TEXT
#undef OPENAI_TOOL_READ_FILE
#undef openai_tool_is_replace
#undef openai_tool_find
#undef openai_tool_descriptor
#undef openai_verify
#undef openai_selftest
#undef openai_status
#undef getenv
