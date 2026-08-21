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
void openai_selftest(agent_state *state);
void openai_status(const agent_state *state);
void openai_verify(agent_state *state);
void openai_show_log(void);
void openai_show_old_log(void);
void openai_clear_log(void);
void openai_show_metrics(void);
void openai_show_state(void);
void openai_clear_state(void);

/* Temporary M267 source compatibility for remaining live consumers. */
#define openai_agent llm_agent
#define openai_agent_write llm_agent_write
#define openai_agent_create llm_agent_create
#define openai_agent_fix llm_agent_fix
#define openai_agent_retry llm_agent_retry
#define openai_agent_build llm_agent_build

#endif
