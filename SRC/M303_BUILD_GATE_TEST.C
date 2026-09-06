#include <stdio.h>
#include <string.h>

#include "agent.h"
#include "COMMAND_BUILD_GATE.INC"

static int expect_gate(
    agent_state *state,
    const char *approval,
    int expected)
{
    return m303_build_gate(state, approval) == expected;
}

int main(void)
{
    agent_state state;
    int ok;

    state.running = 1;
    state.project_root = ".";
    state.api_key_defined = 0;
    state.write_enabled = 1;
    state.dcl_enabled = 1;

    ok =
        expect_gate(
            &state,
            "read-only",
            M303_BUILD_GATE_POLICY) &&
        expect_gate(
            &state,
            "workspace",
            M303_BUILD_GATE_OK) &&
        expect_gate(
            &state,
            "full",
            M303_BUILD_GATE_OK) &&
        expect_gate(
            &state,
            "autopilot",
            M303_BUILD_GATE_OK);

    state.write_enabled = 0;
    ok = ok &&
        expect_gate(
            &state,
            "workspace",
            M303_BUILD_GATE_WRITE);

    state.write_enabled = 1;
    state.dcl_enabled = 0;
    ok = ok &&
        expect_gate(
            &state,
            "workspace",
            M303_BUILD_GATE_DCL);

    ok = ok &&
        expect_gate(
            NULL,
            "workspace",
            M303_BUILD_GATE_POLICY) &&
        expect_gate(
            &state,
            NULL,
            M303_BUILD_GATE_POLICY) &&
        expect_gate(
            &state,
            "dangerous",
            M303_BUILD_GATE_POLICY);

    if (!ok) {
        (void)puts("M303 BUILD gate regression failed.");
        return 2;
    }

    (void)puts("M303 BUILD gate regression passed.");
    return 1;
}
