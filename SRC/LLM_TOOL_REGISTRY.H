#ifndef LLM_TOOL_REGISTRY_H
#define LLM_TOOL_REGISTRY_H

#include "LLM_CACHE.H"

typedef enum llm_tool_kind {
    LLM_TOOL_LIST_DIRECTORY = 1,
    LLM_TOOL_READ_FILE = 2,
    LLM_TOOL_READ_FILE_RANGE = 3,
    LLM_TOOL_SEARCH_FILE = 4,
    LLM_TOOL_REPLACE_TEXT = 5,
    LLM_TOOL_REPLACE_LINES = 6,
    LLM_TOOL_CREATE_FILE = 7,
    LLM_TOOL_RUN_BUILD = 8
} llm_tool_kind;

typedef struct llm_tool_descriptor {
    const char *name;
    llm_tool_kind kind;
    int allows_read;
    int allows_write;
    int requires_confirmation;
    int requires_build;
} llm_tool_descriptor;

const llm_tool_descriptor *llm_tool_find(const char *name);

int llm_tool_is_read(const llm_tool_descriptor *descriptor);
int llm_tool_is_replace(const llm_tool_descriptor *descriptor);

char *llm_tool_execute_read(
    const llm_tool_descriptor *descriptor,
    const char *arguments,
    llm_file_cache_entry *cache);

#endif /* LLM_TOOL_REGISTRY_H */
