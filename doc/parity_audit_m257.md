# M257 Phase 6 Project Instructions Audit

**Baseline:** `main` merge `5ead574e1cfdf672fd66fde8caf7ce7e9fbb7203` (M256 post-merge validated)

This audit reconciles Phase 6 / CAP-024 / CP-019 against the instruction subsystem already present in the repository.

## Validated baseline

M256 and Phase 5 are complete on canonical `main` at `5ead574`.

Post-merge validation on VSI OpenVMS x86-64 showed:

```text
@BUILD
All regression tests passed.
Build completed successfully.

@BUILD_M256
M256 cross-process restart/resume evidence passed.
All M256 session-resumption regressions passed.

$STATUS == "%X00000001"
```

The working tree was clean and synchronized with `origin/main`.

## Authoritative Phase 6 target

CAP-024 and CP-019 remain MISSING.

CP-019 requires OVMS Agent to:

- automatically discover project instruction files;
- apply root and directory-specific instructions;
- report which instructions are active; and
- enforce them during planning and execution.

Phase 6 additionally requires:

1. an OpenVMS-friendly instruction filename;
2. root and directory-specific scope;
3. defined precedence and size limits;
4. active-instruction display; and
5. regressions proving instructions affect planning and execution context.

## Existing instruction foundation

`SRC/openai_instructions.c` already defines the OpenVMS-friendly root instruction filename:

```text
OVMS_AGENT_INSTRUCTIONS.TXT
```

The existing implementation:

- resolves the file relative to `state->project_root`;
- loads at most 4095 bytes into a bounded buffer;
- strips carriage returns and trailing whitespace;
- reports loaded/not-found/error state, source filespec, byte count, truncation, and limit;
- displays the loaded instructions;
- supports explicit reload; and
- composes loaded instructions ahead of the current request under a `PROJECT INSTRUCTIONS` section.

`SRC/openai_agent.c` calls `openai_instr_compose()` before project-map and Git-context composition, so the resulting instruction context already reaches AGENT/PLAN, AGENT/WRITE, AGENT/FIX, and read-only AGENT workflows.

## Existing M232 evidence

`SRC/m232_instr_test.c` proves:

- missing-file state;
- creation and reload of a root instruction file;
- loaded status and no truncation for a small fixture;
- exact instruction display;
- instruction composition into the model request context; and
- parity reporting for project instructions/reload.

This is strong root-scope evidence but does not satisfy CP-019.

## Confirmed Phase 6 gaps

### Root-only discovery

The current loader resolves exactly one `OVMS_AGENT_INSTRUCTIONS.TXT` at the project root (or a test override). It does not walk from the project root toward a target directory and therefore cannot apply directory-specific instructions.

### No scoped precedence

Because only one instruction file can be active, there is no defined precedence between root and nested directory instruction files.

### Active-file reporting is singular

Status reports one filespec and one loaded buffer. CP-019 requires reporting the complete set of active instruction files/scopes.

### Planning/execution evidence is incomplete

M232 proves composition into a generic request buffer, but it does not prove root+nested scope selection or precedence for a target path used by planning/execution.

## M257 implementation direction

Preserve the proven root behavior and extend it rather than replacing it.

Proposed precedence:

1. discover `OVMS_AGENT_INSTRUCTIONS.TXT` at the project root;
2. for a target project-relative path, inspect each parent directory from root toward the target directory;
3. concatenate matching instruction files from broadest to most specific scope;
4. later/more-specific instruction text appears after broader text and therefore has precedence where instructions conflict;
5. enforce a bounded aggregate size and report truncation/refusal deterministically;
6. expose all active instruction files and their order;
7. prove root-only, nested, precedence, missing, oversize, and planning-context behavior with focused OpenVMS regressions.

## Compatibility rule

OpenVMS V7.2 VAX / DEC C remains the target. Every newly introduced externally visible identifier must be 31 characters or fewer before compile/link. Prefer `static` helpers and reuse existing public interfaces where possible.

M257.1 introduces no production C identifiers or linker-visible symbols.
