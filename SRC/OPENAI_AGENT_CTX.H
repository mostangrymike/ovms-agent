#ifndef OPENAI_AGENT_CTX_H
#define OPENAI_AGENT_CTX_H

#include <stdio.h>

void openai_agent_ctx_reset(void);
int openai_agent_ctx_add_items(const char *items_json);
int openai_agent_ctx_add_tool(const char *call_id,
                              const char *tool_output);
int openai_agent_ctx_write(FILE *file,
                           const char *user_prompt);
int openai_agent_ctx_write_final(FILE *file,
                                 const char *user_prompt);

#endif /* OPENAI_AGENT_CTX_H */
