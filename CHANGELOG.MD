# Changelog

This file records user-visible OVMS Agent milestone changes. The project uses permanent lightweight `mNNN` Git tags for canonical promoted milestone commits and semantic `vX.Y.Z` tags for public releases.

The changelog is maintained from M278 forward. Earlier milestone history remains available through the repository's Git history, issues, and permanent tags.

## Unreleased

- M286 normalizes the remaining mixed-case root documentation filenames to fully uppercase names and reformats `README.MD` as conventional Markdown.
- No product command, SETTINGS, build, or runtime behavior changes are included in M286.

## M289 — Native toolchain profiles and guarded source build execution

Tag: `m289`

- Added bounded `LANGUAGE/<LANGUAGE>.BUILD` native toolchain profiles selected by source extension.
- Added guarded `build_source` execution requiring full approval, guarded writes, and DCL execution.
- Added compiled profiles for MACRO32, Fortran, COBOL, Pascal, C++, and BASIC with native compile/link sequencing and failure propagation.
- Added compile-only Java support with one guarded `JAVAC` phase and no link/run phase.
- Added interpreted Python and Perl support with one guarded interpreter run and no compile/link phase.
- Added DCL procedure profiles through a dedicated validated project-relative `.COM` execution path while preserving the existing rule that ordinary `DCL` rejects `@` command procedures.
- Added focused parser/executor regressions and synchronized tool-schema and OpenVMS HELP documentation.
- Native-accepted every shipped M289 language class on OpenVMS V9.2-3 x86-64; the exact final product/HELP acceptance head was `b114b422cf41bd4eb1409ec47593c837c4406418`.

## 1.0.0 — 2026-08-28 (M284)

Tags: `m284`, `v1.0.0`

- Declared the first stable semantic release of OVMS Agent.
- Set the runtime and `VERSION` command semantic version to `1.0.0`.
- Added the MIT License for public use, modification, and redistribution.
- Aligned installation, build, README, changelog, and release guidance with the 1.0.0 contract.
- Added release acceptance guidance requiring the full canonical OpenVMS/VAX build, regression suite, successful OpenVMS status, and startup/`VERSION` smoke test from the exact candidate.
- Performed a release-blocker audit for stale version strings, TODO/FIXME markers, and obvious committed credential material.
- Added no new user-facing commands or persistent SETTINGS; M284 is a release-hardening milestone rather than a feature milestone.

## M283 — Root project documentation set

Tag: `m283`

- Added `INSTALL.MD`, `BUILD.MD`, `CONTRIBUTING.MD`, `SECURITY.MD`, `ARCHITECTURE.MD`, and `CHANGELOG.MD` at the project root.
- Added a concise documentation index to `README.MD` and corrected its SETTINGS summary to include the M281 autonomous-limit entries.
- Established clearer ownership between README overview content, OpenVMS HELP command/settings reference, build/install guidance, contributor workflow, architecture, and security policy.
- Promoted the reviewed documentation tree without changing source, build, launcher, HELP, or runtime behavior.

## M282 — OpenVMS HELP reference

Tag: `m282`

- Added root-level `OVMS_AGENT.HLP` as a hierarchical OpenVMS HELP source.
- Documented the complete current user-facing command and persistent SETTINGS surface.
- Added practical "when/why to use it" guidance rather than syntax-only command descriptions.
- Added the standing project rule that command and SETTINGS changes must update `OVMS_AGENT.HLP` in the same milestone.
- Validated HELP-library creation and representative topic hierarchy on canonical OpenVMS/VAX.

## M281 — Persistent autonomous limits in SETTINGS

Tag: `m281`

- Added persistent SETTINGS for autonomous model/tool turns and autonomous write actions.
- Added interactive SETTINGS entries 9 and 10.
- Added noninteractive `SETTINGS GET/SET` support for `auto_turns` and `auto_writes`.
- Preserved `OVMS_AGENT_AUTO_TURNS` and `OVMS_AGENT_AUTO_WRITES` as higher-precedence legacy logical overrides.
- Established defaults of 12 turns and 3 writes, with supported ranges of 1-32 turns and 1-8 writes.
- Applied saved changes to the next autonomous run without requiring an OVMS Agent restart.
- Preserved the separate M279 create-action ceiling behavior.

## M280 — Interactive SETTINGS

Tag: `m280`

- Added persistent interactive `SETTINGS` support for normal user configuration.
- Added settings for guarded writes, DCL execution, provider profile, model, output-token limit, approval policy, and network allow/deny policy.
- Added noninteractive `SETTINGS SHOW`, `GET`, `SET`, `TOGGLE`, `RESET`, and `RELOAD` forms.
- Preserved legacy logical-name compatibility with explicit source/override reporting.
- Kept provider credentials/profile storage separate from ordinary settings.

## M279 — Legacy backlog closure

Tag: `m279`

- Closed remaining legacy backlog items carried into the milestone.
- Added or reconciled structured symbol/refactoring and agent planning/safety work that remained outside the earlier promoted baseline.
- Preserved the separate `OVMS_AGENT_AUTO_CREATES` create-action ceiling behavior later relied on by M281 compatibility.
- Promoted the reconciled milestone and retained its permanent tag before branch pruning.

## M278 — GitHub service bridge completion

Tag: `m278`

- Completed the shipped GitHub service bridge for pull-request and issue operations.
- Added issue `help`, `list`, and `create` service behavior alongside the existing PR service.
- Added launcher auto-wiring of `GITHUB_BRIDGE.COM` while preserving explicit caller/system overrides.
- Reused the active saved GitHub profile instead of introducing a second credential store.
- Used protected short-lived authorization/body/response files and verified cleanup across OpenVMS RMS versions.
- Preserved FULL approval for GitHub service mutations.
- Verified live PR/issue service behavior and launcher auto-wiring on OpenVMS/VAX.

## Changelog policy

For future milestones:

- add user-visible changes under `Unreleased` while work is in progress;
- on promotion, move the accepted entries into a new `MNNN` section or semantic release section as appropriate;
- identify permanent milestone and semantic release tags;
- describe behavior and compatibility changes, not every internal commit;
- include documentation/security/build changes when they affect how users or contributors operate the project;
- avoid claiming a GitHub merge SHA was VAX-run unless that exact merge SHA was actually accepted on VAX.
