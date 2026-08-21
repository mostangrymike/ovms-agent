#ifndef OVMS_AGENT_OPENAI_H
#define OVMS_AGENT_OPENAI_H

#include "agent.h"

void openai_ask(agent_state *state, const char *prompt);
void openai_chat(agent_state *state, const char *prompt);
void openai_chat_reset(void);
int openai_chat_active(void);
void openai_review_file(agent_state *state, const char *path);
void llm_agent(agent_state *state, const char *goal);
void llm_agent_write(agent_state *state, const char *goal);
void llm_agent_create(agent_state *state, const char *goal);
void llm_agent_fix(agent_state *state, const char *goal);
void llm_agent_retry(agent_state *state, const char *goal);
void llm_agent_build(agent_state *state, const char *goal);
void llm_selftest(agent_state *state);
void llm_status(const agent_state *state);
void llm_verify(agent_state *state);
void openai_log_event(const char *workflow,
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

/* Temporary M267 source compatibility for remaining live consumers. */
#define openai_agent llm_agent
#define openai_agent_write llm_agent_write
#define openai_agent_retry llm_agent_retry
#define openai_show_log llm_show_log
#define openai_show_old_log llm_show_old_log
#define openai_clear_log llm_clear_log
#define openai_show_metrics llm_show_metrics
#define openai_save_state llm_save_state
#define openai_state_save llm_state_save
#define openai_load_state llm_load_state
#define openai_show_state llm_show_state
#define openai_show_memory llm_show_memory
#define openai_clear_state llm_clear_state
#define openai_agent_mode llm_agent_mode

#endif
