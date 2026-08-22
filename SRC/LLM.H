#ifndef OVMS_AGENT_LLM_H
#define OVMS_AGENT_LLM_H

#include "agent.h"

void llm_ask(agent_state *state, const char *prompt);
void llm_chat(agent_state *state, const char *prompt);
void llm_chat_reset(void);
int llm_chat_active(void);
void llm_review_file(agent_state *state, const char *path);
void llm_agent(agent_state *state, const char *goal);
void llm_agent_write(agent_state *state, const char *goal);
void llm_agent_create(agent_state *state, const char *goal);
void llm_agent_fix(agent_state *state, const char *goal);
void llm_agent_retry(agent_state *state, const char *goal);
void llm_agent_build(agent_state *state, const char *goal);
void llm_agent_mode(agent_state *state,
                    const char *goal,
                    int allow_write,
                    int build_after_write,
                    int workflow);
void llm_selftest(agent_state *state);
void llm_status(const agent_state *state);
void llm_verify(agent_state *state);
void llm_log_event(const char *workflow,
                      const char *event,
                      int status);
void llm_show_log(void);
void llm_show_old_log(void);
void llm_clear_log(void);
void llm_show_metrics(void);
void llm_save_state(void);
void llm_state_save(void);
void llm_load_state(void);
void llm_show_state(void);
void llm_show_memory(void);
void llm_clear_state(void);


#endif
