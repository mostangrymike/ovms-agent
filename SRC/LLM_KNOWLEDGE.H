#ifndef LLM_KNOWLEDGE_H
#define LLM_KNOWLEDGE_H

#include <stddef.h>

#define LLM_KNOWLEDGE_GIT 0x01U
#define LLM_KNOWLEDGE_RMS 0x02U
#define LLM_KNOWLEDGE_DCL 0x04U

unsigned int llm_knowledge_detect(const char *prompt);
char *llm_knowledge_merge(const char *base_instructions,
                          const char *user_prompt);
const char *llm_knowledge_last(void);
size_t llm_knowledge_last_bytes(void);

#endif
