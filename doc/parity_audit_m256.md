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

`doc/codex_parity.md` keeps CAP-023 and CP-018 PARTIAL.

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

`SRC/openai_session.c` already implements a durable session record containing:

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

This is strong evidence for the durable session-management foundation, but not for complete CP-018 task resumption.

## Confirmed Phase 5 gaps

### Resume currently selects an ID only

`openai_session_resume()` validates that the session exists and is not archived, then writes its ID to `OVMS_AGENT_SESSION.CUR`.

It does not restore a saved implementation plan, completed/pending execution steps, or pending plan approval.

### Generic workflow state is too shallow for CP-018

`OVMS_AGENT.STATE` currently persists workflow/build/rollback summary state. It explicitly reports that chat state, prompts, and content are not persisted.

That state is useful observability, but it is not a resumable task checkpoint.

### No active-plan checkpoint is bound to a session

The saved plan lives separately in `OVMS_AGENT_PLAN.TXT`. Current session records do not persist the active plan identity, expected fingerprints, completed step index, or pending steps.

### Approval must not be silently resumed

A session record persists the policy name, but that is not equivalent to a current explicit approval for a particular saved plan. Phase 5 must preserve enough information to show that approval was pending while requiring revalidation/reapproval after restart when appropriate.

### Changed-file detection is not part of SESSION/RESUME

The saved-plan subsystem has file fingerprints and stale-plan validation, but `openai_session_resume()` does not currently connect resumption to that validation path.

## M256.1 focused evidence

M256.1 adds `BUILD_M256.COM` to rerun the existing M228 persistent-session regression after a normal `@BUILD`.

A passing result establishes the current durable session foundation on the active OpenVMS baseline while deliberately leaving CAP-023/CP-018 PARTIAL.

Expected focused output:

```text
Running M256 session-resumption regressions...
Persistent session parity bundle test passed.
M256 M228 persistent-session foundation evidence passed.
M256 active-plan/step/approval resume evidence remains pending.
All implemented M256 foundation regressions passed.
```

## Implementation direction

M256 should extend the existing session format rather than create a parallel session system.

The smallest safe sequence is:

1. define a versioned resumable-task checkpoint associated with a session;
2. persist task objective plus active-plan identity/fingerprint and step progress;
3. record approval as pending/required state, never as reusable authorization;
4. validate plan fingerprints/project state during resume;
5. expose resume status showing completed/pending work and whether reapproval is required;
6. add deterministic restart/resume regressions before promoting CAP-023 or CP-018.

## Compatibility rule

OpenVMS V7.2 VAX / DEC C remains the target. Before every M256 production change, all new externally visible identifiers must be 31 characters or fewer; helper functions should remain `static` whenever possible.

M256.1 introduces no production C identifiers or linker-visible symbols.
