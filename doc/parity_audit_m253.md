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

### CAP-008 / CP-016 - Planned file creation

The authoritative matrix says PARTIAL. Later M150/M150B/M150C/M150D work provides saved-plan `create_file` support, transaction behavior, path validation, rollback, and regressions. `SRC/m251_txn_fail_test.c` also provides an automatic transaction-write failure rollback regression. CAP-008 / CP-016 should be reviewed for reclassification to VERIFIED.

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

## Phase 2 evidence reconciliation

### RMS attribute preservation - VALIDATED

`SRC/M253_FILE_EVIDENCE_TEST.C` snapshots RMS organization, record format, record attributes, and maximum record size before and after delete rollback, rename destination/rollback, and move destination/rollback. It also checks an unrelated sentinel file throughout the transaction sequence.

Validated on VSI OpenVMS x86-64 on the `m253-parity-reconcile` branch:

```text
Running M253 parity evidence regressions...
M253 file-operation RMS/isolation evidence passed.
All M253 parity evidence regressions passed.
$STATUS == "%X00000001"
```

This closes the explicit RMS metadata and unrelated-file isolation evidence gap for the M252 structural operations.

### Structural-operation automatic failure rollback - AWAITING VMS VALIDATION

`SRC/M253_FILE_FAIL_TEST.C` stages each structural operation first, then stages a deliberately oversized `.OPT` write that is rejected by the existing RMS record writer. The expected behavior is that `edit_txn_write()` fails and automatically rolls back the already-applied structural operation.

The focused checks are:
- delete: exact original source filespec restored and failed `.OPT` absent,
- rename: exact original source filespec restored, destination absent, failed `.OPT` absent,
- move: exact original source filespec restored, destination absent, failed `.OPT` absent.

This test requires no production failpoint or production-code change; it reuses the established oversized-record write failure mechanism.

### Authoritative evidence references - PENDING

After the structural failure regression is validated, update `doc/codex_parity.md` so every status change to VERIFIED cites the focused source/test procedure and the M252/M253 OpenVMS validation baselines.

## Remaining M253 work

1. Validate `SRC/M253_FILE_FAIL_TEST.C` through `@BUILD_M253` on OpenVMS.
2. Reconcile CAP-008, CAP-009, CAP-010, CAP-011, CAP-015, CAP-024, and CAP-027 against live source and regressions.
3. Update `doc/codex_parity.md` statuses and acceptance-test evidence only where the pass conditions are actually satisfied.
4. Run normal build plus the focused parity suite on OpenVMS.
5. Use the reconciled authoritative matrix to choose the next implementation milestone.

## Compatibility rule

No production code is introduced by this audit/evidence work. M253 test names are not part of the shipped linker surface. Any future production changes remain subject to the OpenVMS VAX / DEC C 31-character external identifier limit.
