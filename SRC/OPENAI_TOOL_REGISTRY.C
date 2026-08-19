#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "llm_internal.h"
#include "openai_tool_registry.h"

static const openai_tool_descriptor tool_registry[] = {
    { "list_directory", OPENAI_TOOL_LIST_DIRECTORY, 1, 0, 0, 0 },
    { "read_file", OPENAI_TOOL_READ_FILE, 1, 0, 0, 0 },
    { "read_file_range", OPENAI_TOOL_READ_FILE_RANGE, 1, 0, 0, 0 },
    { "search_file", OPENAI_TOOL_SEARCH_FILE, 1, 0, 0, 0 },
    { "replace_text", OPENAI_TOOL_REPLACE_TEXT, 0, 1, 1, 0 },
    { "replace_lines", OPENAI_TOOL_REPLACE_LINES, 0, 1, 1, 0 },
    { "create_file", OPENAI_TOOL_CREATE_FILE, 0, 1, 1, 0 },
    { "run_build", OPENAI_TOOL_RUN_BUILD, 0, 0, 0, 1 },
    { NULL, 0, 0, 0, 0, 0 }
};

const openai_tool_descriptor *openai_tool_find(const char *name)
{
    const openai_tool_descriptor *descriptor;

    if (name == NULL) {
        return NULL;
    }

    for (descriptor = tool_registry;
         descriptor->name != NULL;
         ++descriptor) {
        if (strcmp(name, descriptor->name) == 0) {
            return descriptor;
        }
    }

    return NULL;
}

int openai_tool_is_read(const openai_tool_descriptor *descriptor)
{
    return descriptor != NULL && descriptor->allows_read;
}

int openai_tool_is_replace(const openai_tool_descriptor *descriptor)
{
    return descriptor != NULL &&
           (descriptor->kind == OPENAI_TOOL_REPLACE_TEXT ||
            descriptor->kind == OPENAI_TOOL_REPLACE_LINES);
}

char *openai_tool_execute_read(
    const openai_tool_descriptor *descriptor,
    const char *arguments,
    openai_file_cache_entry *cache)
{
    char *tool_output;

    if (descriptor == NULL || !descriptor->allows_read) {
        return NULL;
    }

    tool_output = NULL;

    switch (descriptor->kind) {
    case OPENAI_TOOL_LIST_DIRECTORY:
    {
        char *display_path;

        display_path = NULL;
        tool_output = execute_list_directory_tool(
            arguments,
            &display_path
        );

        (void)printf(
            "Tool executed: list_directory %s\n",
            display_path != NULL ? display_path : ""
        );
        free(display_path);
        break;
    }

    case OPENAI_TOOL_READ_FILE:
    {
        int cache_hit;
        char *display_path;

        cache_hit = 0;
        display_path = NULL;
        tool_output = execute_read_file_tool(
            arguments,
            cache,
            &cache_hit,
            &display_path
        );

        (void)printf(
            "Tool executed: read_file %s%s\n",
            display_path != NULL ? display_path : "",
            cache_hit ? " [cache]" : ""
        );
        free(display_path);
        break;
    }

    case OPENAI_TOOL_READ_FILE_RANGE:
    {
        char *display_path;
        long display_start;
        long display_end;

        display_path = NULL;
        display_start = 0L;
        display_end = 0L;
        tool_output = execute_read_file_range_tool(
            arguments,
            &display_path,
            &display_start,
            &display_end
        );

        (void)printf(
            "Tool executed: read_file_range %s %ld-%ld\n",
            display_path != NULL ? display_path : "",
            display_start,
            display_end
        );
        free(display_path);
        break;
    }

    case OPENAI_TOOL_SEARCH_FILE:
    {
        char *display_path;
        char *display_pattern;

        display_path = NULL;
        display_pattern = NULL;
        tool_output = execute_search_file_tool(
            arguments,
            &display_path,
            &display_pattern
        );

        (void)printf(
            "Tool executed: search_file %s \"%s\"\n",
            display_path != NULL ? display_path : "",
            display_pattern != NULL ? display_pattern : ""
        );
        free(display_path);
        free(display_pattern);
        break;
    }

    default:
        break;
    }

    return tool_output;
}
