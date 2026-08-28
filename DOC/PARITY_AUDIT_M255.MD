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

The authoritative parity specification requires iterative repair to:

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
`llm_agent_repair()`.

`SRC/LLM_RETRY.C` provides the core loop:

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

## M255.1 focused iterative-loop evidence - VALIDATED

`BUILD_M255.COM` reruns M207, M208, and M209 as focused Phase 4 evidence after the
normal build.

Validated on VSI OpenVMS x86-64 on 15 August 2026. The normal build passed before
the final focused validation:

```text
@BUILD
All regression tests passed.
Build completed successfully.
```

The final focused run ended:

```text
M255 M207 commit/rollback evidence passed.
M255 M208 two-attempt bound/report evidence passed.
M255 M209 context-aware retry evidence passed.
All M255 iterative-repair regressions passed.

$STATUS == "%X00000001"
```

The observed M208/M209 traces showed:

- attempt 1 failure followed by `Plan-wide rollback: PASS`,
- context-aware attempt 2,
- successful commit on attempt 2 in the success scenario,
- `Plan-wide rollback: PASS` after attempt 2 failure,
- the explicit two-attempt terminal message in the exhausted scenario.

This is current OpenVMS evidence for the complete iterative-repair behavioral
contract represented by matrix capability CAP-019 and acceptance test CP-017.

## M255.2 terminal execution report - VALIDATED

Source inspection after M255.1 found one real Phase 4 roadmap gap: the repair
engine persisted outcome evidence and printed terminal one-line messages, but did
not itself produce a terminal execution summary plus final diff.

M255.2 updates `SRC/LLM_RETRY.C` without changing repair semantics. On terminal
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

The final OpenVMS M208 evidence visibly demonstrated both principal terminal
outcomes. The second-attempt success scenario printed:

```text
Repair execution summary:
  Outcome:  committed
  Attempts: 2 of 2
  Build:    status 1 (success)
  Rollback: not-required

Final diff:
Git diff:
```

The exhausted scenario printed:

```text
AGENT/REPAIR reached the two-attempt limit. The final failed transaction was rolled back.

Repair execution summary:
  Outcome:  attempt-limit
  Attempts: 2 of 2
  Build:    status 2 (failure)
  Rollback: succeeded

Final diff:
Git diff:
```

M209 repeated both success and exhaustion paths while retaining the context-aware
retry assertions.

Early M255.2 evidence-driver attempts tried to scrape direct image stdout through
DCL redirection. That capture behavior proved unreliable on the validation host,
while the production report itself was visibly correct. The final focused driver
therefore runs M208 normally and uses the regression's existing deterministic
state/content assertions plus exit status; the terminal report is preserved in the
OpenVMS validation transcript instead of being re-parsed by DCL.

## Phase 4 reconciliation - VERIFIED

M255 now has direct OpenVMS evidence for all five Phase 4 roadmap items:

1. failed-build diagnostics feed repair planning;
2. edit-build-test iteration is bounded to two repair attempts;
3. execution stops on success, unsafe rollback state, or the attempt limit;
4. failed repair transactions roll back before retry or terminal exhaustion;
5. terminal execution summary and final diff are emitted.

The authoritative reconciliation may therefore:

- promote CAP-019 from PARTIAL to VERIFIED,
- promote CP-017 from PARTIAL to VERIFIED,
- mark Phase 4 COMPLETE,
- leave later session, instruction, network, external-tool, and image gaps
  unchanged.

## Compatibility rule

OpenVMS V7.2 VAX / DEC C remains the target. Every production external identifier
introduced by M255 must be 31 characters or fewer.

M255.2 introduced only the reporting helpers `llm_repair_rollback_text` and
`llm_repair_report`, both declared `static`. The later M255 harness fixes added
no production C identifiers. Therefore M255 adds no linker-visible symbol that can
violate the DEC C/VAX 31-character external-identifier limit.
