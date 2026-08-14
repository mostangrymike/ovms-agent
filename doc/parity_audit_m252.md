# M252 Parity Baseline Audit

**Baseline:** `main` merge `93091f3` (M251.25 validated)

This audit refreshes the older M160 parity matrix against the live source before
starting new file-operation work. It does not replace `doc/codex_parity.md`; it
records the deltas that later milestones have already resolved and the next
confirmed gap.

## Reclassified capabilities

### Planned file creation: VERIFIED

The M160 document still describes plan-driven creation as partial. Current source
supports `type=create_file` in saved executable plans. M150 validation accepts the
canonical operation and rejects invalid OLD blocks or missing NEW blocks.
M150B verifies commit, rollback, existing-target rejection, mixed replace/create
rollback, and same-path rejection.

Evidence:
- `SRC/OPENAI_M150_VALIDATE.INC`
- `SRC/OPENAI_M150B_VALIDATE.INC`
- `SRC/OPENAI_M150C_VALIDATE.INC`
- `SRC/OPENAI_M150D_VALIDATE.INC`

### Configurable approval policy: VERIFIED for implemented policy surface

The live parity layer implements read-only, workspace, and full policies, supports
an environment-selected policy plus a session override, and reports the active
policy and source.

Evidence:
- `SRC/openai_parity.c`
- `OVMS_AGENT_APPROVAL_POLICY`
- `AGENT/APPROVAL/SET`

### Project instruction file: IMPLEMENTED

The M160 document calls project instructions missing. Current source discovers
`OVMS_AGENT_INSTRUCTIONS.TXT`, applies a bounded 4096-byte instruction payload to
agent requests, reports instruction status, supports reload, and handles absent or
truncated instruction files.

Evidence:
- `SRC/openai_instructions.c`

This audit does not claim directory-specific instruction inheritance; the current
implementation is project-root scoped.

### External tool extensibility: IMPLEMENTED

The live parity layer exposes guarded MCP tooling with full approval and GitHub
integration added by later milestones. This supersedes the M160 claim that no
external-tool extension protocol exists.

Evidence:
- `SRC/openai_parity.c`
- M247 MCP tool registration
- M250 GitHub tool registration

## Confirmed remaining Phase 2 gap

### Guarded file deletion: MISSING

No `delete_file` tool, saved-plan delete operation, or transaction delete operation
is registered in the current source. `edit_txn` currently stages replacement text
for paths and records whether a path existed before; it does not model deletion as
an operation type.

A correct OpenVMS implementation must preserve file-version semantics and provide
transaction rollback. Recreating equivalent text after deletion is not sufficient
proof of exact-version rollback.

### Rename and move: not yet audited for implementation

No `rename_file` or `move_file` tool was found in the live repository search. These
remain after deletion and should be handled as separate milestones rather than
bundled into the first delete implementation.

## M252 plan

1. **M252.1** - guarded transactional file deletion.
2. M252.2 - guarded rename with rollback.
3. M252.3 - guarded move with rollback.
4. Update the authoritative parity matrix after each capability is validated on
   OpenVMS.

## Compatibility rule

Every newly introduced externally visible DEC C identifier must be 31 characters
or fewer. Static helper names should also remain compact where practical.
