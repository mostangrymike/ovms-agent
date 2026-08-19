#ifndef OPENAI_CACHE_H
#define OPENAI_CACHE_H

/* Transitional compatibility shim. New code should include LLM_CACHE.H. */
#include "LLM_CACHE.H"

#define OPENAI_AGENT_CACHE_SIZE LLM_AGENT_CACHE_SIZE
#define openai_file_cache_entry llm_file_cache_entry
#define openai_cache_init llm_cache_init
#define openai_cache_free llm_cache_free
#define openai_cache_store llm_cache_store

#endif /* OPENAI_CACHE_H */
