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

`SRC/COMMAND.C` recognizes `AGENT/REPAIR` explicitly and dispatches to
`openai_agent_repair()`.

`SRC/OPENAI_RETRY.C` already provides the core loop:

- captures the controlled build result before planning,
- uses current diagnostics as repair-plan evidence,
- creates a deterministic saved transactional plan,
- requires plan approval in normal interactive use,
- applies the plan transactionally,
- rebuilds after each repair,
- rolls back a failed repair transaction,
- allows at most two repair attempts,
- provides attempt 2 with the first rolled-back plan and new diagnostics,
- explicitly tells attempt 2 not to repeat the ineffective first repair,
- stops rather than retrying when rollback safety is not proven.

The attempt bound is literal in production code (`attempt <= 2U`).

## Historical deterministic regressions

### M207 end-to-end transaction evidence

`SRC/m207_repair_e2e_test.c` proves commit on successful rebuild and rollback on
failed rebuild for a two-file transaction.

### M208 bounded iteration evidence

`SRC/m208_repair_retry_test.c` proves both terminal two-attempt paths:

- failed attempt 1 rolls back, attempt 2 succeeds and commits;
- both attempts fail, both roll back, and original contents remain after the
  attempt limit.

It requires exactly three build calls: initial build, attempt-1 rebuild, and
attempt-2 rebuild.

### M209 retry-context evidence

`SRC/m209_repair_context_test.c` proves attempt 2 receives:

- the original user goal,
- the first repair plan,
- the failed first rebuild diagnostics,
- explicit rollback context,
- an instruction not to repeat the ineffective repair.

## M255.1 focused evidence - VALIDATED

`BUILD_M255.COM` reruns M207, M208, and M209 as focused Phase 4 evidence after the
normal build.

Validated on VSI OpenVMS x86-64 on 15 August 2026:

```text
@BUILD
All regression tests passed.
Build completed successfully.

@BUILD_M255
M255 M207 commit/rollback evidence passed.
M255 M208 two-attempt bound evidence passed.
M255 M209 context-aware retry evidence passed.
All M255 iterative-repair regressions passed.

$STATUS == "%X00000001"
```

The observed M208/M209 traces additionally showed:

- attempt 1 failure followed by `Plan-wide rollback: PASS`,
- context-aware attempt 2,
- successful commit on attempt 2 in the success scenario,
- `Plan-wide rollback: PASS` after attempt 2 failure,
- the explicit two-attempt terminal message in the exhausted scenario.

This is current OpenVMS evidence for the complete CP-017 behavioral contract.
CP-017 is therefore ready for promotion to VERIFIED during final M255
reconciliation.

## M255.2 terminal execution report - IMPLEMENTED, AWAITING VMS VALIDATION

Source inspection after M255.1 found one real Phase 4 roadmap gap: the repair
engine persisted outcome evidence and printed terminal one-line messages, but did
not itself produce a terminal execution summary plus final diff.

M255.2 updates `SRC/OPENAI_RETRY.C` without changing repair semantics. On terminal
repair execution outcomes it now prints:

- `Repair execution summary:`,
- terminal outcome,
- attempts used out of the two-attempt maximum,
- exact controlled build status plus odd/even success interpretation,
- rollback state,
- `Final diff:`, followed by the existing read-only `project_git_diff()` output.

Reports are emitted for:

- committed repair success,
- unsafe stop when rollback safety is not proven,
- two-attempt exhaustion after the final rollback.

`BUILD_M255.COM` now captures the M208 output and deterministically verifies that
both the committed and attempt-limit scenarios include the execution-summary and
final-diff evidence. It uses line-by-line DCL text checks rather than relying on
SEARCH no-match status semantics.

M255.2 adds only static helper functions in `OPENAI_RETRY.C`. It introduces no new
linker-visible C identifiers.

## Reconciliation boundary

If the updated normal `@BUILD` and focused `@BUILD_M255` both pass on OpenVMS,
M255 has evidence for all five Phase 4 roadmap items. At that point the final M255
documentation reconciliation may:

- promote CP-017 from PARTIAL to VERIFIED,
- mark Phase 4 COMPLETE,
- leave later session, instruction, network, external-tool, and image gaps
  unchanged.

## Compatibility rule

OpenVMS V7.2 VAX / DEC C remains the target. Every production external identifier
introduced by M255 must be 31 characters or fewer. M255.2 adds no external
identifiers; both reporting helpers are `static`.
