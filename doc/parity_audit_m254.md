# M254 Phase 3 Execution Modes Audit

**Baseline:** `main` merge `91447df` (M253 post-merge validated)

This audit reconciles Phase 3 of `doc/codex_parity.md` against the live repository before adding new execution-mode behavior.

## Validated baseline

M253 is complete on `main` at `91447df`.

Validated on VSI OpenVMS x86-64:
- normal `@BUILD` passed,
- `@BUILD_M253` passed RMS/isolation and automatic structural-operation rollback evidence,
- final DCL `$STATUS` was `%X00000001`,
- working tree was clean and synchronized with `origin/main`.

## Phase 3 roadmap

The authoritative Phase 3 requirements are:

1. explicit read-only and workspace-write policies,
2. approved generic DCL command execution,
3. commands constrained to declared policy boundaries,
4. output and OpenVMS condition capture,
5. a non-interactive execution entry point,
6. deterministic exit status and machine-readable output.

## Existing implementation

### Approval policy surface - IMPLEMENTED, evidence reconciliation required

`SRC/openai_parity.c` implements three explicit approval modes:
- `read-only`,
- `workspace`,
- `full`.

Policy may come from the default, `OVMS_AGENT_APPROVAL_POLICY`, or a session override. The interactive command surface exposes:
- `AGENT/APPROVAL`,
- `AGENT/APPROVAL/SET`,
- `AGENT/APPROVAL/RESET`,
- `AGENT/CONTEXT`,
- `AGENT/EXEC`,
- `AGENT/EXEC/DRY`.

The old CAP-015 text saying policy modes are not configurable is stale. CAP-015 should be promoted only after focused policy-boundary regression evidence is recorded.

### Persistent/session execution - IMPLEMENTED but not equivalent to headless execution

The live command surface includes persistent session operations, `AGENT/SESSION/EXEC`, `AGENT/EXEC/RESUME`, `AGENT/EXEC/FORK`, autonomous-loop status/limits, project context, and Git context. These improve resumability and goal execution, but they are still invoked through the interactive command interpreter.

### Generic approved DCL execution - MISSING

The registered parity tool catalog exposes controlled `run_build` execution and guarded external/GitHub tools, but no generic DCL command tool or user command that satisfies CAP-020.

The implementation must not simply call arbitrary DCL from model text. It needs an explicit policy/gate contract, bounded command length/output, activity logging, OpenVMS odd/even status preservation, and deterministic rejection outside the permitted approval mode.

### Non-interactive/headless entry point - MISSING

`SRC/MAIN.C` currently defines `int main(void)` and continuously reads commands from `stdin`. There is no command-line goal/command argument, batch result envelope, or dedicated deterministic exit-status mapping. Existing `AGENT/EXEC` and session execution commands therefore do not satisfy CAP-021 by themselves.

## M254 implementation plan

### M254.1 - Policy evidence reconciliation

Add focused regressions proving that read-only, workspace, and full policy modes expose or reject effects according to their declared boundaries. If those pass on OpenVMS, update CAP-015 from PARTIAL to VERIFIED.

### M254.2 - Approved generic DCL execution

Add a deliberately explicit DCL execution surface with these minimum properties:
- requires the full approval policy,
- requires the existing DCL gate to be enabled,
- bounded command length,
- bounded captured output,
- exact OpenVMS condition value reported,
- odd/even success interpretation retained,
- activity/tool-result logging,
- no silent execution from ordinary model prose,
- deterministic rejection when policy or gate is insufficient.

This closes CAP-020 only after focused OpenVMS regressions.

### M254.3 - Non-interactive execution

Add a one-shot entry point that can execute a supplied agent command/goal without entering the interactive prompt loop. It must:
- initialize and shut down normal agent state,
- execute exactly one requested operation,
- avoid interactive confirmation prompts,
- emit a bounded machine-readable result mode,
- map success/failure to deterministic OpenVMS-compatible process status,
- retain approval and write/DCL gates rather than bypassing them.

This closes CAP-021 only after focused OpenVMS validation.

## Compatibility rule

All production identifiers introduced by M254 must remain within the OpenVMS V7.2 VAX / DEC C 31-character external identifier limit. Test-only identifiers should follow the same rule where practical. No implementation may rely on compiler truncation.
