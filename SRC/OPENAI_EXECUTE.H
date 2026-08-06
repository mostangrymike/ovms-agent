#ifndef OPENAI_EXECUTE_H
#define OPENAI_EXECUTE_H

#include "agent.h"

void openai_plan_execute(agent_state *state);

int openai_execute_count_ops(
    const char *plan_text,
    unsigned int *operation_count_out);

#endif /* OPENAI_EXECUTE_H */
