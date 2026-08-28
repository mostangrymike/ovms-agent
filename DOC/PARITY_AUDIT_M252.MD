# M252 Parity Baseline Audit

**Current baseline:** `main` merge `6464d7a` (M252.2 validated)

This audit refreshes the older M160 parity matrix against the live source. It does
not replace `doc/codex_parity.md`; it records later capability work and the next
confirmed Phase 2 gap.

## Reclassified capabilities

### Planned file creation: VERIFIED

The M160 document still describes plan-driven creation as partial. Current source
supports `type=create_file` in saved executable plans. M150 validation accepts the
canonical operation and rejects invalid OLD blocks or missing NEW blocks.
M150B verifies commit, rollback, existing-target rejection, mixed replace/create
rollback, and same-path rejection.

Evidence:
- `SRC/LLM_M150_VALIDATE.INC`
- `SRC/LLM_M150B_VALIDATE.INC`
- `SRC/LLM_M150C_VALIDATE.INC`
- `SRC/LLM_M150D_VALIDATE.INC`

### Configurable approval policy: VERIFIED for implemented policy surface

The live parity layer implements read-only, workspace, and full policies, supports
an environment-selected policy plus a session override, and reports the active
policy and source.

Evidence:
- `SRC/llm_parity.c`
- `OVMS_AGENT_APPROVAL_POLICY`
- `AGENT/APPROVAL/SET`

### Project instruction file: IMPLEMENTED

The M160 document calls project instructions missing. Current source discovers
`OVMS_AGENT_INSTRUCTIONS.TXT`, applies a bounded 4096-byte instruction payload to
agent requests, reports instruction status, supports reload, and handles absent or
truncated instruction files.

Evidence:
- `SRC/llm_instructions.c`

This audit does not claim directory-specific instruction inheritance; the current
implementation is project-root scoped.

### External tool extensibility: IMPLEMENTED

The live parity layer exposes guarded MCP tooling with full approval and GitHub
integration added by later milestones. This supersedes the M160 claim that no
external-tool extension protocol exists.

Evidence:
- `SRC/llm_parity.c`
- M247 MCP tool registration
- M250 GitHub tool registration

## M252 file operations

### M252.1 guarded file deletion: VERIFIED

Saved plans support `type=delete_file` for one existing project-relative file with
an explicit positive numeric OpenVMS version. The transaction layer renames the
exact file object to a private same-directory holding filespec during the write
phase. Rollback restores that exact object/version; commit removes the held object.
Unversioned paths, `;0`, wildcard versions, device-qualified paths, parent
traversal, and sensitive paths are rejected.

Validation on VSI OpenVMS x86-64 covered transaction commit/rollback, saved-plan
parser acceptance/rejection, saved-plan parse -> stage -> write -> commit/rollback,
and the combined `@BUILD_M252` regression procedure.

M252.1 merged to `main` as `3c5098d`.

### M252.2 guarded same-directory rename: VERIFIED

Saved plans support `type=rename_file` with `path=` and `target_path=` only. Source
and destination are project-relative exact versions, both use positive numeric
`;version`, both name the same directory, the source must exist, and the destination
must not exist. OLD/NEW payload is forbidden. The first contract allows exactly one
rename operation per plan/transaction.

The transaction renames the exact resolved source object to the requested
destination. Rollback renames the actual resolved destination object back to the
original exact filespec; commit leaves the renamed object in place. Slash-style
project paths such as `SRC/A.C;1` are included in directory classification.

Validation on VSI OpenVMS x86-64 covered transaction commit/rollback, saved-plan
parser acceptance/rejection, saved-plan parse -> stage -> write -> commit/rollback,
normal `@BUILD`, combined `@BUILD_M252`, final success status, and a clean working
tree.

M252.2 merged to `main` as `6464d7a`.

### M252.3 guarded cross-directory move: IMPLEMENTED, pending final combined validation

The `m252-file-move` branch adds `type=move_file` saved-plan operations and
`edit_txn_add_move` transaction support. The move contract is deliberately narrow:
- exactly one move operation per plan/transaction,
- source and destination are project-relative,
- both include explicit positive numeric `;version`,
- source and destination must name different directories,
- source must exist,
- destination must not exist,
- no OLD or NEW payload is allowed.

Move and rename are disjoint contracts: slash-style and native OpenVMS directory
syntax are classified, same-directory changes are rejected by move, and
cross-directory changes are rejected by rename. The transaction uses the same
proven exact-object `rename()` mechanism as M252.2; rollback reverses the resolved
destination filespec to the exact original filespec, while commit leaves the exact
object at the destination.

Validation already completed on VSI OpenVMS x86-64:
- exact-version cross-directory transaction move commit/rollback,
- rename-vs-move directory-boundary regression,
- saved-plan move parser acceptance/rejection,
- saved-plan parse -> stage -> write -> commit/rollback,
- normal `@BUILD` after move executor changes.

Remaining M252.3 release step:
- run the updated nine-test `@BUILD_M252` suite and final branch audit before
  PR/merge.

## M252 plan

1. **M252.1** - guarded transactional file deletion — complete and merged.
2. **M252.2** - guarded same-directory rename with rollback — complete and merged.
3. **M252.3** - guarded cross-directory move with rollback — implemented; final
   combined validation pending.
4. Update the authoritative parity matrix after each capability is validated on
   OpenVMS.

## Compatibility rule

Every newly introduced externally visible DEC C identifier must be 31 characters
or fewer. M252 production additions include `edit_txn_add_delete`,
`edit_txn_add_rename`, and `edit_txn_add_move`; execution regression hooks
`llm_m252_delete_exec`, `llm_m252_rename_exec`, and
`llm_m252_move_exec` are also within the limit. Static helper names should
remain compact where practical.
