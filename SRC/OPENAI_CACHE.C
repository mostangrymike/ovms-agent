#include <stdlib.h>
#include <string.h>

#include "openai_cache.h"

void openai_cache_init(openai_file_cache_entry *cache)
{
    unsigned int index;

    for (index = 0U; index < OPENAI_AGENT_CACHE_SIZE; ++index) {
        cache[index].path = NULL;
        cache[index].content = NULL;
    }
}

void openai_cache_free(openai_file_cache_entry *cache)
{
    unsigned int index;

    for (index = 0U; index < OPENAI_AGENT_CACHE_SIZE; ++index) {
        free(cache[index].path);
        free(cache[index].content);
    }
}

int openai_cache_store(openai_file_cache_entry *cache,
                              const char *path,
                              const char *content)
{
    unsigned int index;
    char *path_copy;
    char *content_copy;

    for (index = 0U; index < OPENAI_AGENT_CACHE_SIZE; ++index) {
        if (cache[index].path == NULL) {
            break;
        }
    }

    if (index == OPENAI_AGENT_CACHE_SIZE) {
        return 0;
    }

    path_copy = malloc(strlen(path) + 1U);
    content_copy = malloc(strlen(content) + 1U);

    if (path_copy == NULL || content_copy == NULL) {
        free(path_copy);
        free(content_copy);
        return 0;
    }

    (void)strcpy(path_copy, path);
    (void)strcpy(content_copy, content);
    cache[index].path = path_copy;
    cache[index].content = content_copy;
    return 1;
}
