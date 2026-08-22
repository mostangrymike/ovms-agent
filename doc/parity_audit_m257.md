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

CAP-024 / CP-019 require OVMS Agent to:

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

The existing M232 implementation defines the OpenVMS-friendly instruction filename:

```text
OVMS_AGENT_INSTRUCTIONS.TXT
```

The preserved base implementation:

- resolves the root file relative to `state->project_root`;
- loads at most 4095 bytes into a bounded buffer;
- strips carriage returns and trailing whitespace;
- reports loaded/not-found/error state, source filespec, byte count, truncation, and limit;
- displays the loaded instructions;
- supports explicit reload; and
- composes loaded instructions ahead of the current request under a `PROJECT INSTRUCTIONS` section.

`SRC/LLM_AGENT.C` calls `llm_instr_compose()` before project-map and Git-context composition, so the resulting instruction context reaches AGENT/PLAN, AGENT/WRITE, AGENT/FIX, and read-only AGENT workflows.

## M232 foundation evidence

`SRC/m232_instr_test.c` proves:

- missing-file state;
- creation and reload of a root instruction file;
- loaded status and no truncation for a small fixture;
- exact instruction display;
- root instruction composition into model request context; and
- parity reporting for project instructions/reload.

M257.1 reran that foundation on the Phase 6 branch after a successful full `@BUILD` and completed with `%X00000001`.

## M257 implementation

M257 preserves the proven root loader in `SRC/LLM_INSTRUCTIONS_BASE.C` and routes the existing lowercase `SRC/llm_instructions.c` build entry through `SRC/LLM_INSTRUCTIONS_M257.C`.

Directory-scoped discovery is additive. For safe project-relative paths found in the current goal, M257:

1. derives parent directory prefixes;
2. probes each prefix for `OVMS_AGENT_INSTRUCTIONS.TXT`;
3. orders matching scopes from broadest to deepest;
4. appends deeper instructions later so they take precedence over conflicting broader guidance;
5. caps scope count, path sizes, each scoped instruction buffer, and aggregate composed instruction data;
6. rejects unsafe directory tokens containing traversal, absolute-device syntax, or backslashes; and
7. records active scoped files plus truncation state in status/show output.

A goal without a safe project-relative path retains the original root-only behavior.

## M257.2 scope and precedence evidence

`SRC/M257_SCOPE_TEST.C` proves:

- the root M232 instruction remains active;
- a task targeting `TEST/M257_TARGET.C` discovers `TEST/OVMS_AGENT_INSTRUCTIONS.TXT` automatically;
- the root rule appears before the scoped rule;
- the model context contains an explicit precedence statement that later/deeper scopes override conflicting broader instructions;
- status reports the active scoped file; and
- a goal with no scoped project path falls back to root-only instructions.

Validated OpenVMS output:

```text
Project instruction context bundle test passed.
M257 M232 root-instruction foundation evidence passed.
M257 directory-scoped instruction precedence test passed.
M257 directory-scope/precedence evidence passed.
All implemented M257 project-instruction regressions passed.

$STATUS == "%X00000001"
```

## M257.3 planning/write enforcement evidence

`SRC/M257_CONTEXT_TEST.C` uses the real production `LLM_AGENT.C` entry path and intercepts only the final `llm_agent_mode()` boundary. No network/API call is required.

The test requires:

- `llm_agent_plan()` to reach the final boundary as `LLM_WORKFLOW_PLAN` with write disabled;
- `llm_agent_write()` to reach the final boundary as `LLM_WORKFLOW_WRITE` with write enabled;
- both captured model goals to contain the root instruction, scoped instruction, precedence marker, and original project-relative request; and
- root instruction text to precede scoped instruction text in both real workflow contexts.

Validated OpenVMS output:

```text
M257 planning/write instruction enforcement test passed.
M257 planning/write enforcement evidence passed.
All M257 project-instruction regressions passed.

$STATUS == "%X00000001"
```

The working tree remained clean and synchronized with `origin/m257-project-instructions`.

## Acceptance reconciliation

M257 now satisfies the full CP-019 pass condition:

- automatic project-instruction discovery: verified;
- root instruction scope: verified by M232/M257.1;
- directory-specific scope: verified by M257.2;
- deterministic broad-to-specific precedence: verified by M257.2;
- bounded loading/composition and truncation reporting: implemented and exercised by the root/scoped loaders;
- active instruction reporting: verified by M257.2; and
- planning/write enforcement: verified by M257.3 through the real production entry path.

Therefore:

- **CAP-024 Project instruction files -> VERIFIED**
- **CP-019 Project instructions -> VERIFIED**
- **Phase 6 - Project instructions and configuration -> COMPLETE**

This does not change CAP-025, CAP-026, CAP-027, or CP-020. Practical Codex parity is not yet complete.

## VAX compatibility audit

OpenVMS V7.2 VAX / DEC C remains the target. M257 was audited against the 31-character external identifier limit.

New externally visible base aliases introduced by the wrapper remain within the limit; scoped implementation helpers are `static`. The repository also avoids a case-distinct `LLM_INSTRUCTIONS.C` / `llm_instructions.c` collision: the existing lowercase source path is the canonical build entry.
