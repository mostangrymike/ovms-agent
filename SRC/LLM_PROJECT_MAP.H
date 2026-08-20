#ifndef LLM_PROJECT_MAP_H
#define LLM_PROJECT_MAP_H

#include <stddef.h>
#include "AGENT.H"

int llm_project_refresh(const agent_state *state);
int llm_project_map_text(const agent_state *state,
                         char *output, size_t output_size);
int llm_project_files_text(const agent_state *state,
                           char *output, size_t output_size);
int llm_project_src_text(const agent_state *state,
                         char *output, size_t output_size);
int llm_project_tests_text(const agent_state *state,
                           char *output, size_t output_size);
int llm_project_build_text(const agent_state *state,
                           char *output, size_t output_size);
int llm_project_context(const agent_state *state,
                        char *output, size_t output_size);
int llm_project_compose(const agent_state *state,
                        const char *goal,
                        char *output, size_t output_size);
void llm_show_project_map(const agent_state *state, int filter);
void llm_show_project_ctx(const agent_state *state);
void llm_project_refresh_cmd(const agent_state *state);

#endif /* LLM_PROJECT_MAP_H */
