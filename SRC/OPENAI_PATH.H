#ifndef OPENAI_PATH_H
#define OPENAI_PATH_H

/* Transitional compatibility shim. New code should include LLM_PATH.H. */
#include "LLM_PATH.H"

#define openai_path_is_safe llm_path_is_safe
#define openai_contains_ignore_case llm_contains_ignore_case
#define openai_path_is_sensitive llm_path_is_sensitive
#define openai_listing_entry_hidden llm_listing_entry_hidden

#endif /* OPENAI_PATH_H */
