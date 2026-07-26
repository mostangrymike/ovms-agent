#include <stdio.h>
#include <string.h>
#include "command.h"
#include "command_internal.h"
#include "util.h"

void move_test_helper(agent_state *state, const char *arguments)
{
    (void)state;
    (void)arguments;
    (void)puts("move test");
}

