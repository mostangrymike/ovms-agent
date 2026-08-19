#ifndef OPENAI_TOOL_REGISTRY_H
#define OPENAI_TOOL_REGISTRY_H

#include "LLM_CACHE.H"

typedef enum openai_tool_kind {
    OPENAI_TOOL_LIST_DIRECTORY = 1,
    OPENAI_TOOL_READ_FILE = 2,
    OPENAI_TOOL_READ_FILE_RANGE = 3,
    OPENAI_TOOL_SEARCH_FILE = 4,
    OPENAI_TOOL_REPLACE_TEXT = 5,
    OPENAI_TOOL_REPLACE_LINES = 6,
    OPENAI_TOOL_CREATE_FILE = 7,
    OPENAI_TOOL_RUN_BUILD = 8
} openai_tool_kind;

typedef struct openai_tool_descriptor {
    const char *name;
    openai_tool_kind kind;
    int allows_read;
    int allows_write;
    int requires_confirmation;
    int requires_build;
} openai_tool_descriptor;

const openai_tool_descriptor *openai_tool_find(const char *name);

int openai_tool_is_read(const openai_tool_descriptor *descriptor);
int openai_tool_is_replace(const openai_tool_descriptor *descriptor);

char *openai_tool_execute_read(
    const openai_tool_descriptor *descriptor,
    const char *arguments,
    llm_file_cache_entry *cache);

#endif /* OPENAI_TOOL_REGISTRY_H */
