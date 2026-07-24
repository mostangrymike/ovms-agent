#ifndef OVMS_AGENT_OPENAI_H
#define OVMS_AGENT_OPENAI_H

#include "agent.h"

void openai_ask(agent_state *state, const char *prompt);
void openai_chat(agent_state *state, const char *prompt);
void openai_chat_reset(void);
int openai_chat_active(void);
void openai_review_file(agent_state *state, const char *path);

#endif
