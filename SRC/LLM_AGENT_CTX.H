#ifndef LLM_AGENT_CTX_H
#define LLM_AGENT_CTX_H

#include <stdio.h>

void llm_agent_ctx_reset(void);
int llm_agent_ctx_add_items(const char *items_json);
int llm_agent_ctx_add_tool(const char *call_id,
                           const char *tool_output);
int llm_agent_ctx_write(FILE *file,
                        const char *user_prompt);
int llm_agent_ctx_write_final(FILE *file,
                              const char *user_prompt);

#endif /* LLM_AGENT_CTX_H */
