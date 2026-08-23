#ifndef LLM_SSE_H
#define LLM_SSE_H

#include <stdio.h>

typedef void (*llm_sse_text_cb)(const char *text);

int llm_sse_parse(FILE *input,
                  llm_sse_text_cb text_callback,
                  const char *response_path,
                  int *text_emitted);

#endif /* LLM_SSE_H */
