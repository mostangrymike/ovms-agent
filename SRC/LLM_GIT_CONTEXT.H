#ifndef LLM_GIT_CONTEXT_H
#define LLM_GIT_CONTEXT_H

#include <stddef.h>
#include "agent.h"

int llm_git_refresh(const agent_state *state);
int llm_git_status_text(const agent_state *state,
                        char *output,
                        size_t output_size);
int llm_git_diff_text(const agent_state *state,
                      char *output,
                      size_t output_size);
int llm_git_changed_text(const agent_state *state,
                         char *output,
                         size_t output_size);
int llm_git_context(const agent_state *state,
                    char *output,
                    size_t output_size);
int llm_git_compose(const agent_state *state,
                    const char *goal,
                    char *output,
                    size_t output_size);
void llm_show_git_status(const agent_state *state);
void llm_show_git_diff(const agent_state *state);
void llm_show_git_changed(const agent_state *state);
void llm_show_git_context(const agent_state *state);
void llm_git_refresh_cmd(const agent_state *state);
int llm_git_rms_copy(const char *path, const char *target);
void llm_test_git_data(const char *status_text,
                       const char *diff_text);

#endif /* LLM_GIT_CONTEXT_H */
