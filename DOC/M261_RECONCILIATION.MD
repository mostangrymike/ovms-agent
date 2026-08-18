# M261 VMSBBS Hardening Reconciliation

## Purpose

M261 reconciles the validated VMSBBS-driven OVMS Agent hardening work onto the canonical M260 `main` line without merging the divergent `m253-vmsbbs-gaps` history or removing capabilities already completed in M252 through M260.

## Why reconciliation is required

The historical `m253-vmsbbs-gaps` branch contains useful validated work, but its Git history diverged substantially from canonical `main`. A tree-level comparison during repository housekeeping showed that replacing M260 `main` with that branch tree would remove later validation drivers and capabilities, including parts of execution, session resumption, network/external-tool support, and image input.

For that reason, the historical branch is reference evidence only. It must not be merged or squashed wholesale into `main`.

`m261-vmsbbs-hardening` was created from current canonical `main` and reset to the exact M260 main commit before reconciliation work began.

## Scope for M261

Port only these validated VMSBBS hardening behaviors onto the canonical M260 architecture:

1. GAP-001 — recognize COBOL `.COB`, `.COBOL`, and `.CBL` files as source code, while treating `.CPY` copy files as supporting/dependency material. **Ported and accepted on OpenVMS 2026-08-18.**
2. GAP-002 — after a successful guarded write, discard saved file-reading results so later reads reopen the newest OpenVMS RMS file version. **Ported and accepted on OpenVMS 2026-08-18; full regression build passed.**
3. GAP-003 — track successful guarded automatic writes across the whole run and restore every affected file to its exact pre-run RMS version if the run stops incomplete because of an automatic limit or error. Do not make an extra final-synthesis request after a partial write has already reached its limit. **Ported with deterministic RMS rollback coverage and accepted on OpenVMS 2026-08-18.**
4. GAP-004 — make normal top-level `BUILD` failures available to repair planning through the existing persisted failed-build record, while removing temporary capture files and stale failure versions correctly. **Ported unchanged from the validated historical fix and accepted on OpenVMS 2026-08-18.**
5. GAP-005 — before supervised exact replacements reach the confirmation/write path, reject a candidate that removes an explicitly required goal term. Explicit user requests to remove or disable that feature remain allowed. **Ported with deterministic M251.10 coverage and accepted on OpenVMS 2026-08-18; full regression build passed.**

## Deliberately separate work

The broad provider-neutral architecture, Requesty support, persistent artificial-intelligence provider profiles, persistent GitHub profiles, and guarded saved-profile Git authentication on `m253-vmsbbs-gaps` are not part of this M261 reconciliation.

Those changes require a separate follow-on branch from the new canonical main so they can be reconciled without hiding architectural changes inside a VMSBBS bug-hardening pull request.

## Compatibility requirements

- Preserve all canonical M252-M260 functionality and validation drivers.
- Keep `BUILD.COM` as the authoritative build and regression path.
- Preserve OpenVMS RMS file-version semantics.
- Preserve OpenVMS V7.2 VAX and older DEC C compatibility targets where applicable.
- Every new linker-visible function or global name must be 31 characters or fewer.
- Do not introduce implicit function declarations.
- Do not introduce a project `STRING.H` or `STRING.C` that could shadow the DEC C library header.

## Acceptance before merge

The M261 reconciliation gates have been satisfied:

- `@BUILD` passes on OpenVMS;
- deterministic regression coverage for the reconciled GAP-001 through GAP-005 behavior passes;
- previously validated live OpenVMS behavior was reused where repeating it would add no new evidence;
- comparison against `main` shows no unintended removal of M252-M260 capabilities or milestone validation drivers;
- accidental Git mode changes introduced during connector writes were identified and corrected;
- `README.md` and `features.md` describe the resulting user-visible behavior in plain technical language; and
- the pull request is ready for final review and a squash merge so temporary/no-op reconciliation commits do not enter `main`.

## Historical branch handling

Keep `m253-vmsbbs-gaps` available as reference until both M261 and the later provider-neutral reconciliation are complete. New GAP-006 and later development should not continue accumulating on that historical branch.
