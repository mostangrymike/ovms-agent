# M253 Post-M252 Parity Reconciliation Audit

**Baseline:** `main` merge `0efa5bd` (M252.3 post-merge validated)

This audit records the parity state after completion of the M252 guarded file-operation series. Its purpose is to reconcile the authoritative M160-era `doc/codex_parity.md` matrix with capabilities and evidence that now exist in the repository, while keeping unproven acceptance requirements explicitly open.

## Canonical M252 result

M252 is complete on `main` at `0efa5bd`.

Validated on VSI OpenVMS x86-64:
- normal `@BUILD` passed,
- `@BUILD_M252` passed all nine focused regressions,
- delete transaction/parser/saved-plan execution passed,
- same-directory rename transaction/parser/saved-plan execution passed,
- cross-directory move transaction/parser/saved-plan execution passed,
- final DCL `$STATUS` was `%X00000001`,
- working tree was clean and synchronized with `origin/main`.

The M252 implementation preserves the OpenVMS exact-version object model for delete rollback, rename, and move rather than reconstructing equivalent text.

## Authoritative matrix entries that are stale

The following classifications in `doc/codex_parity.md` no longer describe the live repository and should be reconciled after evidence review.

### CAP-008 / CP-016 - Planned file creation

The authoritative matrix says PARTIAL. Later M150/M150B/M150C/M150D work provides saved-plan `create_file` support, transaction behavior, path validation, rollback, and regressions. This should be reviewed for reclassification to VERIFIED.

### CAP-009 - File deletion

The authoritative matrix says MISSING. M252.1 now provides guarded exact-version `delete_file` saved-plan execution with transaction rollback and focused regressions. Feature presence is no longer missing.

### CAP-010 - File rename

The authoritative matrix says UNKNOWN. M252.2 now provides guarded same-directory exact-version `rename_file` saved-plan execution with rollback and focused regressions.

### CAP-011 - File move

The authoritative matrix says UNKNOWN. M252.3 now provides guarded cross-directory exact-version `move_file` saved-plan execution with rollback and focused regressions.

### CAP-015 - Guarded workspace-write workflow

The authoritative matrix says PARTIAL because policy modes were not configurable. Later parity work implements read-only, workspace, and full approval policies with environment and session selection. The current policy surface should be reviewed against the original CAP-015 pass condition before reclassification.

### CAP-024 / CP-019 - Project instructions

The authoritative matrix says MISSING. The live repository implements project-root `OVMS_AGENT_INSTRUCTIONS.TXT` discovery, bounded loading, status reporting, reload, and request integration. However, the original CP-019 pass condition also requires directory-specific instruction scope. Root-level instructions are implemented; full CP-019 parity is therefore not automatically VERIFIED.

### CAP-027 / CP-020 external-tool portion

The authoritative matrix says MISSING. Later parity work adds guarded MCP and GitHub external-tool registration. The external-tool portion is implemented, but CP-020 combines external tools with image input, so CP-020 as a whole cannot be marked VERIFIED while image input remains missing.

## Phase 2 evidence still requiring explicit reconciliation

The M160 roadmap lists more than feature presence for guarded file operations. Before declaring Phase 2 complete, verify the following requirements against existing tests and add focused evidence where absent.

1. **Failure injection for each file operation.** M252 has commit/rollback and malformed/rejected-operation coverage. Determine whether delete, rename, and move each have an intentional mid-operation failure regression proving rollback after a write-phase failure rather than only explicit rollback requests.

2. **RMS attribute preservation for every operation.** Exact-object rename/move should preserve file metadata by construction, and delete rollback restores the held exact object. Record a regression that explicitly checks organization, record format, record attributes, and carriage-control behavior so the parity claim is evidence-backed rather than inferred.

3. **Unrelated-file isolation.** The parity evidence rules require confirmation that unrelated files are not modified. Existing transaction tests should be checked for this assertion and extended if necessary.

4. **Authoritative evidence references.** Every status changed to VERIFIED should cite the focused source/test procedure and the post-merge validation baseline.

## Proposed M253 work

1. Reconcile CAP-008, CAP-009, CAP-010, CAP-011, CAP-015, CAP-024, and CAP-027 against live source and regressions.
2. Add any missing Phase 2 failure-injection / RMS-preservation / isolation regressions.
3. Update `doc/codex_parity.md` statuses and acceptance-test evidence only after those proofs exist.
4. Run normal build plus the relevant focused parity suite on OpenVMS.
5. Use the reconciled authoritative matrix to choose the next implementation milestone, rather than following stale M160 status text.

## Compatibility rule

No production code is introduced by this audit. Future M253 code changes remain subject to the OpenVMS VAX / DEC C 31-character external identifier limit.
