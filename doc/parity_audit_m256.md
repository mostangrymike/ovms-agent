# M256 Phase 5 Session Resumption Audit

**Baseline:** `main` merge `36808aae500c70d6c24af0a69283e809c42ecede` (M255 post-merge validated)

This audit reconciles the Phase 5 session-resumption requirement against the persistent-session implementation already present in the repository.

## Validated baseline

M255 and Phase 4 are complete on canonical `main` at `36808aa`.

Post-merge validation on VSI OpenVMS x86-64 showed:

```text
@BUILD
All regression tests passed.
Build completed successfully.

@BUILD_M255
M255 M207 commit/rollback evidence passed.
M255 M208 two-attempt bound/report evidence passed.
M255 M209 context-aware retry evidence passed.
All M255 iterative-repair regressions passed.

$STATUS == "%X00000001"
```

The working tree was clean and synchronized with `origin/main`.

## Authoritative Phase 5 target

Before M256 reconciliation, `doc/codex_parity.md` kept CAP-023 and CP-018 PARTIAL.

CP-018 requires restart/resume of an unfinished task with:

- task identity and objective,
- active plan,
- completed steps,
- pending steps,
- pending approval state without silently reusing stale approval,
- relevant context,
- changed-file detection before resumption.

The Phase 5 roadmap also calls for resume, list-session, and abandon-session operations.

## Existing persistent-session foundation

`SRC/llm_session.c` already implements a durable session record containing:

- 8-character session ID,
- archived state,
- created/updated timestamps,
- parent session ID,
- approval-policy name,
- human-readable session name,
- original goal,
- current goal,
- execution count.

The subsystem persists records in `OVMS_AGENT_SESSIONS.DAT` and the selected session in `OVMS_AGENT_SESSION.CUR`.

Existing commands include:

- `AGENT/SESSION/NEW`,
- `AGENT/SESSION/LIST`,
- `AGENT/SESSION/SHOW`,
- `AGENT/SESSION/RESUME`,
- `AGENT/SESSION/FORK`,
- rename/archive/unarchive/delete/current,
- transcript/results/export commands,
- session execution and execution-resume/fork commands.

## M228 deterministic evidence

`SRC/m228_session_test.c` already proves:

- creation and current-session selection,
- persistence of original/current goal,
- persistence of policy label and execution count,
- fork inheritance,
- rename,
- list visibility,
- archive blocks resume,
- unarchive permits resume,
- delete clears a current session and removes it from the list.

This establishes the durable session-management foundation used by M256.

## M256.1 foundation validation

M256.1 added `BUILD_M256.COM` to rerun the existing M228 persistent-session regression after a normal `@BUILD`.

Observed OpenVMS evidence:

```text
Running M256 session-resumption regressions...
Persistent session parity bundle test passed.
M256 M228 persistent-session foundation evidence passed.
M256 active-plan/step/approval resume evidence remains pending.
All implemented M256 foundation regressions passed.

$STATUS == "%X00000001"
```

## M256.2 resumed-session approval safety

`SRC/LLM_SESSION_M256.C` wraps the mature session implementation so a successful session resume clears saved-plan approval through `llm_plan_session_reset()`.

The deterministic M256 test simulates stale in-process approval state before resume and verifies that resume leaves:

- `llm_plan_approved == 0`,
- `llm_approved_hash == 0`,
- `llm_approval_invalidated == 0`.

This preserves the existing rule that approval is explicit and session-local: a resumed task may remember that work is pending, but it cannot silently reuse an old authorization.

Observed focused evidence:

```text
M256 session resume reapproval evidence passed.
M256 resumed-session reapproval evidence passed.
All implemented M256 session-resumption regressions passed.

$STATUS == "%X00000001"
```

## M256.3 active-plan checkpoint and stale-file refusal

`SRC/LLM_CHECKPOINT.C`, included by the M256 session wrapper, persists a minimal resumable-plan checkpoint in `OVMS_AGENT_CHECKPOINTS.DAT` containing:

- persistent session ID,
- saved-plan checksum,
- total operation count,
- completed operation count.

For an unfinished atomic saved-plan transaction the checkpoint records `completed=0` and `pending=N`. A successful plan transaction is all-or-nothing, so there is no supported partially committed transaction state to resume.

On resume, the checkpoint path requires:

1. the same saved-plan checksum;
2. an active saved plan;
3. all saved-plan file fingerprints to remain current.

If any fingerprinted planned file changes, resume fails before selecting the target session. Approval is cleared regardless and must be reacquired before execution.

Observed OpenVMS evidence after the project-relative fixture correction:

```text
Checkpoint restored: completed 0 of 1, pending 1, approval pending.
Checkpoint restored: completed 0 of 1, pending 1, approval pending.
Checkpoint stale: saved plan or planned files changed; regenerate the plan.
M256 session resume reapproval/checkpoint evidence passed.
M256 resumed-session reapproval/checkpoint evidence passed.
M256 stale planned-file resume refusal evidence passed.
All implemented M256 session-resumption regressions passed.

$STATUS == "%X00000001"
```

## M256.4 real restart/resume acceptance evidence

The remaining CP-018 gap was explicit process restart. M256.4 adds two separate test images so no process-local globals can leak across the boundary:

- `M256_RESTART_SETUP_TEST.EXE` creates a persistent session, records its objective, creates and binds an active saved plan checkpoint, and exits successfully while leaving only durable state on disk.
- `M256_RESTART_RESUME_TEST.EXE` starts as a fresh process, restores the session and objective from disk, resumes the same active-plan checkpoint, verifies completed/pending progress, and verifies that plan approval is still pending rather than reused.

The final OpenVMS run produced:

```text
Checkpoint restored: completed 0 of 1, pending 1, approval pending.
M256 restart setup evidence passed.
Checkpoint restored: completed 0 of 1, pending 1, approval pending.
M256 cross-process restart/resume evidence passed.
M256 cross-process restart/resume evidence passed.
All M256 session-resumption regressions passed.

$STATUS == "%X00000001"
```

The working tree was synchronized with `origin/m256-session-resumption` at validation time.

## Phase 5 acceptance conclusion

M256 plus the pre-existing M228 session lifecycle now satisfies the Phase 5 requirements:

1. **Task identity and objective:** persistent session ID/name and original/current goal survive restart.
2. **Active plan and completed steps:** saved plan plus bound checkpoint survive restart; unfinished atomic work is represented as `0/N` completed with `N` pending.
3. **Pending approvals:** resume explicitly clears authorization and reports approval pending; old approval is never silently reused.
4. **Changed-file detection:** saved-plan checksum and file fingerprints are validated before checkpoint resumption; stale planned files refuse continuation.
5. **Resume/list/abandon lifecycle:** existing session resume/list plus archive/delete lifecycle remain covered by M228.
6. **Real restart boundary:** two-image M256.4 evidence proves recovery from durable files rather than process-local state.

Therefore CAP-023 and CP-018 may be promoted to VERIFIED and Phase 5 may be marked COMPLETE.

## Compatibility audit

OpenVMS V7.2 VAX / DEC C remains the target.

M256 keeps helper functions `static` where possible. The wrapper aliases introduced for the mature session implementation are:

- `llm_sess_resume_base` — 23 characters;
- `llm_sess_resume_cmd_base` — 27 characters.

Both remain below the DEC C/VAX 31-character external identifier limit. The checkpoint implementation and M256 test additions introduce no known production external identifier longer than 31 characters.

Later unresolved capabilities remain unchanged by this reconciliation: CAP-024 through CAP-027 and CP-019 through CP-020 are not promoted by M256.
