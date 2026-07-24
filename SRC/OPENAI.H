#ifndef OVMS_AGENT_OPENAI_H
#define OVMS_AGENT_OPENAI_H

#include "agent.h"

void openai_ask(agent_state *state, const char *prompt);
void openai_chat(agent_state *state, const char *prompt);
void openai_chat_reset(void);
int openai_chat_active(void);
void openai_review_file(agent_state *state, const char *path);
void openai_agent(agent_state *state, const char *goal);
void openai_agent_write(agent_state *state, const char *goal);
void openai_agent_fix(agent_state *state, const char *goal);
void openai_agent_retry(agent_state *state, const char *goal);
void openai_agent_build(agent_state *state, const char *goal);

#endif
