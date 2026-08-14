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

### Approval policy surface - IMPLEMENTED, focused evidence added

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

`SRC/openai_transcript.c` independently classifies direct tools by minimum policy. `READ`-class tools are read-only, `EDIT`/`PATCH`/`BUILD` require workspace, and `RUN`/`MCP` require full. The direct runner also applies the write gate to workspace/full effects and a separate DCL gate to `RUN`.

M254 strengthens `SRC/m229_tool_test.c` so it proves these boundaries without executing DCL:
- read-only refuses `EDIT`,
- workspace reaches the write-gate path for `BUILD`,
- workspace refuses full-only `RUN`,
- full policy reaches the independent DCL gate for `RUN`.

If the strengthened regression passes on OpenVMS, the old CAP-015 text saying policy modes are not configurable is stale and CAP-015 can be promoted to VERIFIED.

### Persistent/session execution - IMPLEMENTED but not equivalent to headless execution

The live command surface includes persistent session operations, `AGENT/SESSION/EXEC`, `AGENT/EXEC/RESUME`, `AGENT/EXEC/FORK`, autonomous-loop status/limits, project context, and Git context. These improve resumability and goal execution, but they are still invoked through the interactive command interpreter.

### Restricted approved DCL execution - IMPLEMENTED, CAP-020 remains incomplete

A guarded `RUN` path already exists in `SRC/COMMAND.C` and is reachable as a full-policy direct tool through `AGENT/TOOL/RUN RUN ...`.

The current implementation:
- requires `state->dcl_enabled`,
- is full-policy through the direct tool runner,
- applies the write gate before dispatch,
- rejects command separators, procedure invocation, redirection, and a broad set of mutating DCL verbs,
- allows a bounded read-oriented whitelist (`SHOW`, `DIRECTORY`, `SEARCH`, `TYPE`, `DIFFERENCES`, and read-only Git subcommands),
- captures command output through a temporary command procedure,
- reports the OpenVMS completion status,
- asks interactively for confirmation before execution.

This is useful controlled DCL execution, but it does **not** satisfy CAP-020's current wording, "Arbitrary approved DCL execution." It is intentionally read-oriented and interactive. M254.2 therefore needs to decide and implement the bounded meaning of "arbitrary approved" without weakening the existing safety boundary.

### Non-interactive/headless entry point - MISSING

`SRC/MAIN.C` currently defines `int main(void)` and continuously reads commands from `stdin`. There is no command-line goal/command argument, batch result envelope, or dedicated deterministic exit-status mapping. Existing `AGENT/EXEC` and session execution commands therefore do not satisfy CAP-021 by themselves.

## M254 implementation plan

### M254.1 - Policy evidence reconciliation

Validate the strengthened `m229_tool_test.c` on OpenVMS. If it passes, update CAP-015 from PARTIAL to VERIFIED with the policy-boundary evidence.

### M254.2 - Approved generic DCL execution

Extend the deliberately explicit DCL execution surface while preserving these minimum properties:
- requires the full approval policy,
- requires the existing write and DCL gates,
- bounded command length,
- bounded captured output,
- exact OpenVMS condition value reported,
- odd/even success interpretation retained,
- activity/tool-result logging,
- no silent execution from ordinary model prose,
- deterministic rejection when policy or gate is insufficient,
- no implicit bypass of the existing restricted `RUN` safety policy.

CAP-020 closes only after the exact approved-command contract is defined and focused OpenVMS regressions pass.

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
