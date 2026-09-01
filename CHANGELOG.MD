# Changelog

This file records user-visible OVMS Agent milestone changes. Permanent lightweight `mNNN` tags identify canonical promoted milestone commits; public releases additionally use semantic `vX.Y.Z` tags.

The changelog is maintained from M278 forward. Earlier history remains available through Git history, issues, and historical records under `DOC/`.

## Unreleased

No additional user-visible product changes are currently recorded beyond the M293 documentation-currentness work below.

## M293 — Markdown documentation currentness

Tag: `m293` on promotion

- Audited every repository `*.MD` file against promoted M292 product state.
- Updated living root documentation to identify OpenVMS V9.2-3 x86-64 as the current canonical native acceptance host while preserving historical DEC C/VAX compatibility constraints where they remain part of the source contract.
- Synchronized current documentation for M289 native toolchain profiles/`build_source`, M291 atomic ordinary `AGENT/WRITE` and Git/RMS/auth hardening, and M292 provider response-health diagnostics.
- Reframed frozen parity material so M252-M260/Codex-parity documents are not mistaken for current product command documentation.
- Preserved historical reconciliation/audit facts and language-pack evidence rather than rewriting the platform terminology of old milestones.
- No source, build procedure, launcher, HELP command surface, SETTINGS behavior, or runtime behavior changes.

## M292 — Provider health test diagnostics

Tag: `m292`

- Added `PROVIDER TEST` to test all configured provider profiles and `PROVIDER TEST name` to test one profile without changing the active provider.
- Sends one bounded real plain-text no-op request through each profile's configured transport and reports model, elapsed time, and textual state.
- Responsive profiles report `OK`; failures distinguish `CONFIG`, `LOCAL ERROR`, `TRANSPORT`, `API ERROR`, `EMPTY`, and `INVALID`.
- Added green/red ANSI presentation while retaining textual states for plain terminals/logs.
- Uses dedicated temporary request/header/response files, removes all RMS versions after the probe, and never prints credentials.
- Added provider-free deterministic response-classification regressions and synchronized `OVMS_AGENT.HLP`.
- Native-accepted exact candidate `a732840d4f5195b950dbcdb67b14bebe24af64b7` on OpenVMS V9.2-3 x86-64; promoted main `c6eda68f573e407d405a539874f518853b50f86c` carries the identical tree `76b9ffe15dd3b52cf3efb08c684fee01a2327db6`.

## M291 — Remaining reliability backlog hardening

Tag: `m291`

- Resolved the remaining reliability/workflow backlog tracked by #154 while keeping provider/model compatibility as separate work.
- Added local confirmation for model-invoked `structured_patch` in guarded write workflows.
- Added conservative COBOL one-for-one paragraph-rename protection.
- Cleared transient ANSI turn status before durable request-construction diagnostics.
- Hardened `GITAUTH --check` into a non-mutating current-origin write-authorization preflight; setup retains a config-only check.
- Filtered OVMS Agent's own exact Git scratch basenames from synthesized untracked status without hiding tracked same-name paths/lookalikes.
- Made ordinary `AGENT/WRITE` existing-file editing atomic through one `structured_patch`; sequential replace primitives remain supervised `AGENT/FIX` behavior.
- Reverified the historical Git-discovery report and closed it `not_planned` after a fresh valid OpenVMS worktree proved current discovery works and the historical metadata was structurally incomplete.
- Native-accepted exact candidate `b94aee71eeec0d991273f60012bbf491f934d5e4`; promoted main `82fd65c9c6b75eead02387d7ed1812ca3280bc4e` carries the identical accepted tree.

## M290 — Reliability hardening and backlog reduction

Tag: `m290`

- Hardened RMS structured-patch integrity.
- Corrected subprocess/Git status propagation so encoded POSIX nonzero exits are not mistaken for native odd-success statuses.
- Fixed Git/RMS direct-read short-read behavior.
- Isolated persistent-settings regressions from user settings.
- Added bounded provider transport timeout behavior.
- Completed the remaining reliability-backlog audit scoped by #152.
- Native-accepted exact development head `8e1c96bfcf02d1d8114644ea0a2ee34a5b520bb9`; promoted main `587574a7d68e09a86f8975d6b889ea4ccf45675f` carries the identical accepted tree.

## M289 — Native toolchain profiles and guarded source build execution

Tag: `m289`

- Added bounded `LANGUAGE/<LANGUAGE>.BUILD` native toolchain profiles selected by source extension.
- Added guarded `build_source` execution requiring full approval, guarded writes, and DCL execution.
- Added compiled profiles for MACRO32, Fortran, COBOL, Pascal, C++, and BASIC with native compile/link sequencing and failure propagation.
- Added compile-only Java support, interpreted Python/Perl support, and validated project-relative DCL procedure execution.
- Added focused parser/executor regressions and synchronized tool-schema/HELP documentation.
- Native-accepted every shipped M289 language class on OpenVMS V9.2-3 x86-64; final product/HELP acceptance head was `b114b422cf41bd4eb1409ec47593c837c4406418`.

## M288 — Shared OpenVMS language knowledge packs

Tag: `m288`

- Added data-driven `LANGUAGE/<LANGUAGE>.MD` knowledge packs that can be injected into AI requests when strong language evidence is present.
- Added a 16 KiB pack bound and an explicit policy boundary preventing language guidance from overriding sandbox, safety, evidence, approval, or write rules.
- Added native-validated COBOL and Fortran packs derived from multi-language campaign evidence.
- Added provider-free detection/injection regressions and launcher support through `OVMS_AGENT_LANGUAGE_DIR`.
- Kept unrelated requests unchanged when no confident language/matching pack exists.

## M286 — Uppercase root documentation and Markdown README

Tag: `m286`

- Normalized the remaining root documentation filenames to uppercase `.MD` names.
- Reworked `README.MD` into conventional Markdown and updated live root-document cross-references.
- Preserved historical `DOC` records instead of rewriting them solely for filename-case modernization.
- No product command, SETTINGS, build, or runtime behavior changes.

## M285 — Canonical uppercase DOC history directory

Tag: `m285`

- Consolidated duplicate-case documentation history so uppercase `DOC` is the sole canonical documentation-history directory.
- Moved the Codex parity specification and M252-M260 parity audits into uppercase `DOC` while preserving their historical contents.
- Removed the obsolete lowercase `doc` tree.
- No product/runtime behavior changes.

## 1.0.0 — 2026-08-28 (M284)

Tags: `m284`, `v1.0.0`

- Declared the first stable semantic release of OVMS Agent.
- Set the runtime and `VERSION` command semantic version to `1.0.0`.
- Added the MIT License.
- Aligned installation/build/README/changelog/release guidance with the 1.0.0 contract.
- Added release acceptance guidance requiring the then-canonical OpenVMS/VAX build, regressions, successful OpenVMS status, and startup/`VERSION` smoke test from the exact candidate.
- Performed a release-blocker audit for stale version strings, TODO/FIXME markers, and obvious committed credential material.
- M284 was release hardening rather than a new command/SETTINGS feature milestone.

## M283 — Root project documentation set

Tag: `m283`

- Added the root installation, build, contributor, security, architecture, and changelog documents.
- Added a documentation index to README and aligned its SETTINGS summary.
- Established ownership between README overview, HELP command/settings reference, build/install guidance, contributor workflow, architecture, security, and changelog.

## M282 — OpenVMS HELP reference

Tag: `m282`

- Added root-level `OVMS_AGENT.HLP` as hierarchical OpenVMS HELP source.
- Documented the then-current user-facing commands and persistent SETTINGS surface with practical when/why guidance.
- Established the durable rule that command/SETTINGS changes update HELP in the same milestone.
- Validated HELP-library creation and representative topic hierarchy on the canonical OpenVMS system used for M282.

## M281 — Persistent autonomous limits in SETTINGS

Tag: `m281`

- Added persistent `auto_turns` and `auto_writes` SETTINGS with interactive entries and noninteractive GET/SET support.
- Preserved higher-precedence legacy logical overrides.
- Established defaults of 12 turns / 3 writes and supported ranges of 1-32 / 1-8.

## M280 — Interactive SETTINGS

Tag: `m280`

- Added persistent interactive SETTINGS for guarded writes, DCL execution, provider/model, output-token limit, approval policy, network policy, and related noninteractive forms.
- Preserved legacy logical-name compatibility and kept credentials separate from ordinary settings.

## M279 — Legacy backlog closure

Tag: `m279`

- Closed the remaining legacy defect backlog carried after M278.
- Reconciled structured symbol/refactoring and agent planning/safety work outside the earlier baseline.
- Preserved separate create-action ceiling behavior later used by autonomous SETTINGS compatibility.

## M278 — GitHub service bridge completion

Tag: `m278`

- Completed the shipped GitHub service bridge for pull-request and issue operations.
- Added launcher auto-wiring while preserving explicit overrides.
- Reused the active saved GitHub profile rather than adding a second credential store.
- Used protected short-lived authorization/body/response files with RMS-version cleanup.
- Preserved FULL approval for GitHub service mutations.
- Verified live PR/issue service behavior on the OpenVMS acceptance environment used at that milestone.

## Changelog policy

For future milestones:

- record in-progress user-visible work under `Unreleased` or the pending milestone section;
- on promotion, identify the permanent milestone tag and semantic release tag when applicable;
- describe behavior/compatibility/security/build/documentation changes that matter to users/contributors rather than every internal commit;
- preserve historical platform/evidence facts rather than retroactively rewriting old acceptance records; and
- distinguish an exact native-tested candidate SHA from a later GitHub merge SHA that merely carries an identical accepted tree.
