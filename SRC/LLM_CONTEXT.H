#ifndef LLM_CONTEXT_H
#define LLM_CONTEXT_H

#include <stddef.h>

int llm_context_evidence_text(const char *session,
                              char *output,
                              size_t output_size);
int llm_context_build(const char *goal,
                      char *output,
                      size_t output_size);
int llm_context_current(char *output,
                        size_t output_size);
void llm_show_context_current(void);

#endif /* LLM_CONTEXT_H */
