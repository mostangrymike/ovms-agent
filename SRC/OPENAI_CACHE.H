#ifndef OPENAI_CACHE_H
#define OPENAI_CACHE_H

#define OPENAI_AGENT_CACHE_SIZE 8

typedef struct openai_file_cache_entry {
    char *path;
    char *content;
} openai_file_cache_entry;

void openai_cache_init(openai_file_cache_entry *cache);
void openai_cache_free(openai_file_cache_entry *cache);
int openai_cache_store(openai_file_cache_entry *cache,
                       const char *path,
                       const char *content);

#endif /* OPENAI_CACHE_H */
