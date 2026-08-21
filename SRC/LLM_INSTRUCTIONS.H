#ifndef LLM_INSTRUCTIONS_H
#define LLM_INSTRUCTIONS_H

#include <stddef.h>
#include "agent.h"

int llm_instr_reload(const agent_state *state);
int llm_instr_compose(const agent_state *state,
                      const char *goal,
                      char *output,
                      size_t output_size);
int llm_instr_status_text(const agent_state *state,
                          char *output,
                          size_t output_size);
int llm_instr_show_text(const agent_state *state,
                        char *output,
                        size_t output_size);
void llm_show_instr_status(const agent_state *state);
void llm_show_instr(const agent_state *state);
void llm_instr_reload_cmd(const agent_state *state);
void llm_test_instr_path(const char *path);

#endif /* LLM_INSTRUCTIONS_H */
