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

## M254.1 - Policy evidence reconciliation - VALIDATED

`SRC/openai_parity.c` implements three explicit approval modes:
- `read-only`,
- `workspace`,
- `full`.

Policy may come from the default, `OVMS_AGENT_APPROVAL_POLICY`, or a session override. `SRC/openai_transcript.c` independently classifies direct tools by minimum policy and applies the write/DCL gates.

M254 strengthened `SRC/m229_tool_test.c` to prove:
- read-only refuses `EDIT`,
- workspace reaches the write-gate path for `BUILD`,
- workspace refuses full-only `RUN`,
- full policy reaches the independent DCL gate for `RUN`.

Validated on VSI OpenVMS x86-64 on `m254-execution-modes`:

```text
Building OVMS Agent Version 2...
All regression tests passed.
Build completed successfully.
$STATUS == "%X00000001"
```

This closes the CAP-015 policy evidence gap. The authoritative matrix should promote CAP-015 to VERIFIED during M254 reconciliation.

## Existing restricted RUN path

A guarded interactive `RUN` path already exists in `SRC/COMMAND.C`. It requires the DCL gate, is full-policy when invoked through the direct tool runner, applies the write gate, captures output/status, and deliberately allows only read-oriented DCL. It remains unchanged by M254.2.

## M254.2 - Approved generic DCL execution - IMPLEMENTED, AWAITING VMS VALIDATION

M254 adds `SRC/COMMAND_DCL.C` and registers the explicit `DCL` command. This is a separate full-authority surface rather than a weakening of restricted `RUN`.

Contract:
- requires `full` approval policy,
- requires `state->write_enabled`,
- requires `state->dcl_enabled`,
- accepts one bounded printable DCL command,
- rejects command-procedure invocation and wrapper-control verbs that could escape status capture,
- executes through a private `SYS$SCRATCH` command procedure,
- captures bounded output,
- preserves the exact OpenVMS condition value,
- reports odd/even success interpretation,
- records transcript/tool-result evidence and activity-log evidence,
- is reachable only through the explicit `DCL` command, not ordinary model prose,
- leaves the older restricted interactive `RUN` policy unchanged.

The strengthened `SRC/m229_tool_test.c` now additionally verifies:
- workspace-policy DCL refusal,
- full-policy DCL-gate refusal,
- a real harmless `SHOW DEFAULT` execution,
- non-empty captured output,
- odd OpenVMS success status,
- persisted DCL result evidence,
- rejection of wrapper-control `EXIT`.

The test redirects transcript and activity logging to test-only files so normal user history is not modified.

New production external identifiers are `command_dcl` and `command_dcl_exec`; both are below the OpenVMS VAX / DEC C 31-character limit.

## M254.3 - Non-interactive/headless entry point - STILL OPEN

`SRC/MAIN.C` still defines `int main(void)` and continuously reads commands from `stdin`. There is no command-line one-shot goal/command argument, bounded machine-readable result envelope, or dedicated deterministic exit-status mapping.

M254.3 must:
- initialize and shut down normal agent state,
- execute exactly one supplied operation without entering the prompt loop,
- avoid interactive confirmation prompts,
- emit bounded machine-readable output,
- map success/failure to deterministic OpenVMS-compatible process status,
- retain approval and write/DCL gates rather than bypassing them.

## Compatibility rule

All production identifiers introduced by M254 must remain within the OpenVMS V7.2 VAX / DEC C 31-character external identifier limit. No implementation may rely on compiler truncation.
