#include <stdio.h>
#include <string.h>

#include "agent.h"
#include "COMMAND_BUILD_GATE.INC"
#include "M303_WORKFLOW_GATE.INC"

static int expect_build_gate(
    agent_state *state,
    const char *approval,
    int expected)
{
    return m303_build_gate(state, approval) == expected;
}

static int expect_write_gate(
    agent_state *state,
    const char *approval,
    int expected)
{
    return m303_write_gate(state, approval) == expected;
}

static int expect_workflow_gate(
    agent_state *state,
    const char *approval,
    int require_dcl,
    int expected)
{
    return m303_workflow_gate(
        state,
        approval,
        require_dcl
    ) == expected;
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
        expect_build_gate(
            &state,
            "read-only",
            M303_BUILD_GATE_POLICY) &&
        expect_build_gate(
            &state,
            "workspace",
            M303_BUILD_GATE_OK) &&
        expect_build_gate(
            &state,
            "full",
            M303_BUILD_GATE_OK) &&
        expect_build_gate(
            &state,
            "autopilot",
            M303_BUILD_GATE_OK) &&
        expect_write_gate(
            &state,
            "read-only",
            M303_BUILD_GATE_POLICY) &&
        expect_write_gate(
            &state,
            "workspace",
            M303_BUILD_GATE_OK) &&
        expect_workflow_gate(
            &state,
            "read-only",
            0,
            M303_WORKFLOW_POLICY) &&
        expect_workflow_gate(
            &state,
            "workspace",
            0,
            M303_WORKFLOW_OK) &&
        expect_workflow_gate(
            &state,
            "workspace",
            1,
            M303_WORKFLOW_OK) &&
        expect_workflow_gate(
            &state,
            "full",
            1,
            M303_WORKFLOW_OK) &&
        expect_workflow_gate(
            &state,
            "autopilot",
            1,
            M303_WORKFLOW_OK);

    state.write_enabled = 0;
    ok = ok &&
        expect_build_gate(
            &state,
            "workspace",
            M303_BUILD_GATE_WRITE) &&
        expect_write_gate(
            &state,
            "workspace",
            M303_BUILD_GATE_WRITE) &&
        expect_workflow_gate(
            &state,
            "workspace",
            0,
            M303_WORKFLOW_WRITE) &&
        expect_workflow_gate(
            &state,
            "workspace",
            1,
            M303_WORKFLOW_WRITE);

    state.write_enabled = 1;
    state.dcl_enabled = 0;
    ok = ok &&
        expect_build_gate(
            &state,
            "workspace",
            M303_BUILD_GATE_DCL) &&
        expect_write_gate(
            &state,
            "workspace",
            M303_BUILD_GATE_OK) &&
        expect_workflow_gate(
            &state,
            "workspace",
            0,
            M303_WORKFLOW_OK) &&
        expect_workflow_gate(
            &state,
            "workspace",
            1,
            M303_WORKFLOW_DCL);

    ok = ok &&
        expect_build_gate(
            NULL,
            "workspace",
            M303_BUILD_GATE_POLICY) &&
        expect_build_gate(
            &state,
            NULL,
            M303_BUILD_GATE_POLICY) &&
        expect_build_gate(
            &state,
            "dangerous",
            M303_BUILD_GATE_POLICY) &&
        expect_write_gate(
            NULL,
            "workspace",
            M303_BUILD_GATE_POLICY) &&
        expect_write_gate(
            &state,
            NULL,
            M303_BUILD_GATE_POLICY) &&
        expect_write_gate(
            &state,
            "dangerous",
            M303_BUILD_GATE_POLICY) &&
        expect_workflow_gate(
            NULL,
            "workspace",
            0,
            M303_WORKFLOW_POLICY) &&
        expect_workflow_gate(
            &state,
            NULL,
            0,
            M303_WORKFLOW_POLICY) &&
        expect_workflow_gate(
            &state,
            "dangerous",
            0,
            M303_WORKFLOW_POLICY);

    if (!ok) {
        (void)puts("M303 command gate regression failed.");
        return 2;
    }

    (void)puts("M303 command gate regression passed.");
    return 1;
}
