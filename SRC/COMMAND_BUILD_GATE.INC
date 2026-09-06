#define M303_BUILD_GATE_OK 0
#define M303_BUILD_GATE_POLICY 1
#define M303_BUILD_GATE_WRITE 2
#define M303_BUILD_GATE_DCL 3

static int m303_write_gate(
    const agent_state *state,
    const char *approval_name)
{
    if (state == NULL ||
        approval_name == NULL ||
        (strcmp(approval_name, "workspace") != 0 &&
         strcmp(approval_name, "full") != 0 &&
         strcmp(approval_name, "autopilot") != 0)) {
        return M303_BUILD_GATE_POLICY;
    }

    if (!state->write_enabled) {
        return M303_BUILD_GATE_WRITE;
    }

    return M303_BUILD_GATE_OK;
}

static int m303_build_gate(
    const agent_state *state,
    const char *approval_name)
{
    int gate;

    gate = m303_write_gate(state, approval_name);
    if (gate != M303_BUILD_GATE_OK) {
        return gate;
    }

    if (!state->dcl_enabled) {
        return M303_BUILD_GATE_DCL;
    }

    return M303_BUILD_GATE_OK;
}
