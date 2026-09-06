#define M303_WORKFLOW_OK 0
#define M303_WORKFLOW_POLICY 1
#define M303_WORKFLOW_WRITE 2
#define M303_WORKFLOW_DCL 3

static int m303_workflow_gate(
    const agent_state *state,
    const char *approval_name,
    int require_dcl)
{
    if (state == NULL ||
        approval_name == NULL ||
        (strcmp(approval_name, "workspace") != 0 &&
         strcmp(approval_name, "full") != 0 &&
         strcmp(approval_name, "autopilot") != 0)) {
        return M303_WORKFLOW_POLICY;
    }

    if (!state->write_enabled) {
        return M303_WORKFLOW_WRITE;
    }

    if (require_dcl && !state->dcl_enabled) {
        return M303_WORKFLOW_DCL;
    }

    return M303_WORKFLOW_OK;
}
