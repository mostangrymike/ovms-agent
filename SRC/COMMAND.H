#ifndef OVMS_AGENT_COMMAND_H
#define OVMS_AGENT_COMMAND_H

#include "agent.h"

void command_prompt(void);
void command_execute(agent_state *state, char *input);
void command_run(agent_state *state, const char *arguments);
void command_changeset(agent_state *state, const char *arguments);

#endif /* OVMS_AGENT_COMMAND_H */
