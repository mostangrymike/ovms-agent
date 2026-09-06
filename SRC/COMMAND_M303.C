#include <stdio.h>

#define command_execute m303_command_execute_raw
#include "COMMAND.C"
#undef command_execute

#include "COMMAND_CHANGE_GATE.INC"

static const char *(*m303_change_approval_fn)(void) = NULL;

void m303_cmd_set_change_approval(
    const char *(*callback)(void))
{
    m303_change_approval_fn = callback;
}

void command_execute(agent_state *state, char *input)
{
    const char *approval;

    if (m303_changeset_apply_input(input)) {
        approval = m303_change_approval_fn != NULL ?
            m303_change_approval_fn() : NULL;

        if (!m303_change_policy_ok(approval)) {
            (void)puts(
                "CHANGESET APPLY refused: workspace approval policy required."
            );
            return;
        }
    }

    m303_command_execute_raw(state, input);
}
