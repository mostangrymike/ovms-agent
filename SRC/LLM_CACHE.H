#ifndef LLM_CACHE_H
#define LLM_CACHE_H

#define LLM_AGENT_CACHE_SIZE 8

typedef struct llm_file_cache_entry {
    char *path;
    char *content;
} llm_file_cache_entry;

void llm_cache_init(llm_file_cache_entry *cache);
void llm_cache_free(llm_file_cache_entry *cache);
int llm_cache_store(llm_file_cache_entry *cache,
                    const char *path,
                    const char *content);

#endif /* LLM_CACHE_H */
