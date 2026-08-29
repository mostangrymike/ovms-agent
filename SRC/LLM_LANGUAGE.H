#ifndef LLM_LANGUAGE_H
#define LLM_LANGUAGE_H

#include <stddef.h>

const char *llm_lang_detect(const char *prompt,
                            const char *project_root);
char *llm_lang_merge(const char *base_instructions,
                     const char *user_prompt);
const char *llm_lang_last(void);
size_t llm_lang_last_bytes(void);

#endif /* LLM_LANGUAGE_H */
