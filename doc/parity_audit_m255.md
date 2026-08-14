# M255 Phase 4 Iterative Repair Audit

**Baseline:** `main` merge `55b3dc5` (M254 post-merge validated)

This audit reconciles the Phase 4 iterative-engineering requirement against the
repair implementation and regressions that already exist in the repository.

## Validated baseline

M254 and Phase 3 are complete on `main` at `55b3dc5`.

Post-merge validation on VSI OpenVMS x86-64 showed:

```text
@BUILD
All regression tests passed.
Build completed successfully.

@BUILD_M254
M254 headless DCL policy/status evidence passed.
M254 preserved condition: %X00030001
All M254 execution-mode regressions passed.

$STATUS == "%X00000001"
```

The working tree was clean and synchronized with `origin/main`.

## Authoritative Phase 4 target

`doc/codex_parity.md` keeps CP-017 PARTIAL with this pass condition:

- inspect a failed build,
- produce a bounded repair,
- execute it after required approval,
- rebuild,
- repeat until success or a defined attempt limit is reached.

The Phase 4 roadmap additionally requires:

1. diagnostic parsing connected to repair planning,
2. bounded edit-build-test iteration,
3. stop on success, repeated failure, unsafe action, or attempt limit,
4. rollback guarantees across iterations,
5. an execution summary and final diff.

## Existing repair engine

The repository already contains more of CP-017 than the authoritative status
currently credits.

### `AGENT/REPAIR`

`SRC/COMMAND.C` recognizes `AGENT/REPAIR` explicitly and dispatches to
`openai_agent_repair()`.

`SRC/OPENAI_RETRY.C` implements the repair workflow. Relevant behavior includes:

- captures the current controlled build result before planning,
- builds a repair prompt from the current diagnostics,
- creates a deterministic saved transactional repair plan,
- requires plan approval in normal interactive use,
- applies the saved plan transactionally,
- rebuilds after the repair,
- rolls back a failed repair transaction,
- allows at most two repair attempts,
- provides attempt 2 with the first rolled-back plan and the new diagnostics,
- explicitly tells attempt 2 not to repeat the same ineffective repair.

The two-attempt bound is literal in the implementation (`attempt <= 2U`).

### M207 end-to-end transaction evidence

`SRC/m207_repair_e2e_test.c` supplies deterministic plan/build hooks and proves:

- failed-build diagnostics enter the repair workflow,
- a two-file transactional repair commits when the controlled rebuild succeeds,
- the same two-file transaction is rolled back when the controlled rebuild fails.

Success message:

```text
End-to-end deterministic AGENT/REPAIR test passed.
```

### M208 bounded iteration evidence

`SRC/m208_repair_retry_test.c` proves both terminal paths of the two-attempt loop:

- first repair fails, is rolled back, second repair succeeds and commits;
- first repair fails and rolls back, second repair also fails, and original
  contents are restored after the defined attempt limit.

It additionally requires exactly three controlled build calls in each scenario:
initial failure, attempt-1 rebuild, attempt-2 rebuild.

Success message:

```text
Bounded two-attempt AGENT/REPAIR test passed.
```

### M209 retry-context evidence

`SRC/m209_repair_context_test.c` proves the second repair prompt contains:

- the original user goal,
- the first repair plan,
- diagnostics from the failed first rebuild,
- explicit rollback context,
- an instruction not to repeat the same ineffective repair.

It repeats the second-attempt success and final-rollback state checks from M208.

Success message:

```text
Context-aware second-attempt AGENT/REPAIR test passed.
```

## M255.1 evidence reconciliation

M255.1 adds `BUILD_M255.COM` as a focused evidence driver over the existing M207,
M208, and M209 regression images produced by the normal `@BUILD`.

If the focused driver passes on OpenVMS after the normal build, the repository
will have direct current evidence for the complete CP-017 behavioral contract:

- failed diagnostics -> planning,
- approval-aware transactional repair,
- rebuild,
- rollback after failed attempt,
- context-aware retry,
- success stop,
- hard two-attempt stop.

At that point CP-017 should be promoted from PARTIAL to VERIFIED during M255
reconciliation unless current source inspection uncovers a contrary acceptance
requirement.

## Remaining Phase 4 reconciliation

CP-017 and the full Phase 4 roadmap are not identical. After M255.1 validation,
M255 must separately verify whether current persisted repair history/activity
already satisfies the roadmap's final execution-summary requirement and whether a
user-facing final diff is emitted or needs a small implementation addition.

Phase 4 must not be marked COMPLETE until those roadmap items are evidenced.

## Compatibility rule

OpenVMS V7.2 VAX / DEC C remains the target. Every production external identifier
introduced by M255 must be 31 characters or fewer. M255.1 introduces no production
C identifiers or linker-visible symbols.
