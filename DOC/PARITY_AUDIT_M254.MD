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

`SRC/llm_parity.c` implements three explicit approval modes:
- `read-only`,
- `workspace`,
- `full`.

Policy may come from the default, `OVMS_AGENT_APPROVAL_POLICY`, or a session override. `SRC/llm_transcript.c` independently classifies direct tools by minimum policy and applies the write/DCL gates.

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

## M254.3 - Non-interactive/headless entry point - VALIDATED

M254.3 adds a one-shot DCL entry through `OVMS_AGENT.COM`:

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

`BUILD_M254.COM` proves both denial and success paths. The denial case uses workspace policy and requires the deterministic M254 failure condition. The success case uses full policy plus both write and DCL gates, executes harmless `SHOW DEFAULT`, verifies `executed:true`, extracts the JSON condition field, compares that condition numerically with the exact process status returned by `@OVMS_AGENT`, and requires `success:true`.

Validated on VSI OpenVMS x86-64:

```text
Running M254 execution-mode regressions...
M254 headless DCL policy/status evidence passed.
M254 preserved condition: %X00030001
All M254 execution-mode regressions passed.
$STATUS == "%X00000001"
```

The `%X00030001` result is intentionally retained as evidence that M254 does not normalize successful OpenVMS conditions to `%X00000001`; it preserves the actual odd condition returned by the executed DCL command and reports it consistently in JSON and process status.

All M254.3 helper functions added in `SRC/MAIN.C` are `static`, so M254.3 adds no new linker-visible identifiers. The only new M254 production external identifiers remain `command_dcl` and `command_dcl_exec`, both within the DEC C/VAX 31-character external identifier limit.

This supplies the implementation/evidence needed to promote CAP-021 to VERIFIED during final M254 reconciliation for the defined non-interactive one-shot command interface.

## Compatibility rule

All production identifiers introduced by M254 must remain within the OpenVMS V7.2 VAX / DEC C 31-character external identifier limit. No implementation may rely on compiler truncation.
