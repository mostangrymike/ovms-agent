#ifndef OVMS_AGENT_COMMAND_H
#define OVMS_AGENT_COMMAND_H

#include "agent.h"

void command_prompt(void);
void command_execute(agent_state *state, char *input);

#endif /* OVMS_AGENT_COMMAND_H */
