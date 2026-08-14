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

This closes the CAP-015 policy evidence gap. The authoritative matrix should promote CAP-015 to VERIFIED during final M254 reconciliation.

## Existing restricted RUN path

A guarded interactive `RUN` path already exists in `SRC/COMMAND.C`. It requires the DCL gate, is full-policy when invoked through the direct tool runner, applies the write gate, captures output/status, and deliberately allows only read-oriented DCL. It remains unchanged by M254.2.

## M254.2 - Approved generic DCL execution - VALIDATED

M254 adds `SRC/COMMAND_DCL.C` and registers the explicit top-level `DCL` command. This is a separate full-authority surface rather than a weakening of restricted `RUN`.

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

`SRC/m229_tool_test.c` verifies:
- workspace-policy DCL refusal,
- full-policy DCL-gate refusal,
- a real harmless `SHOW DEFAULT` execution,
- non-empty captured output,
- odd OpenVMS success status,
- persisted DCL result evidence,
- rejection of wrapper-control `EXIT`.

The test redirects transcript and activity logging to test-only files so normal user history is not modified.

Validated on VSI OpenVMS x86-64 after commit `3718988`:

```text
Building OVMS Agent Version 2...
All regression tests passed.
Build completed successfully.
$STATUS == "%X00000001"
```

New production external identifiers are `command_dcl` and `command_dcl_exec`; both are below the OpenVMS VAX / DEC C 31-character limit.

This supplies the implementation/evidence needed to promote CAP-020 to VERIFIED during final M254 reconciliation. The explicit `DCL` command is the authoritative M254.2 interface; direct tool-table exposure is not required for the CAP-020 contract and must not be claimed unless separately wired and tested.

## M254.3 - Non-interactive/headless entry point - IN PROGRESS

The first M254.3 slice adds a one-shot DCL entry through `OVMS_AGENT.COM`:

```text
@OVMS_AGENT "DCL SHOW DEFAULT" JSON
```

The one-shot contract is:
- initialize and shut down normal agent state,
- execute exactly one supplied DCL operation without entering the prompt loop,
- do not prompt for confirmation or consume interactive input,
- require the same full approval, write, and DCL gates as interactive approved DCL,
- emit one bounded JSON result object when `JSON` mode is requested,
- include the exact OpenVMS condition value and odd/even interpretation,
- return the captured OpenVMS condition as the one-shot process result,
- use a quiet initialization mode so JSON output is not contaminated by the normal banner.

This slice proves deterministic headless command execution. CAP-021 remains open until the one-shot interface is validated on OpenVMS and the final acceptance wording is reconciled against whether a direct tool/command entry is sufficient or a model-goal entry is also required.

## Compatibility rule

All production identifiers introduced by M254 must remain within the OpenVMS V7.2 VAX / DEC C 31-character external identifier limit. No implementation may rely on compiler truncation.
