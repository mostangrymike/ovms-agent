# Changelog

This file records user-visible OVMS Agent milestone changes. The project uses permanent lightweight `mNNN` Git tags for canonical promoted milestone commits.

The changelog is maintained from M278 forward. Earlier milestone history remains available through the repository's Git history, issues, and permanent tags.

## Unreleased

### Documentation

- Added a conventional root-level documentation set: `INSTALL.md`, `BUILD.md`, `CONTRIBUTING.md`, `SECURITY.md`, `ARCHITECTURE.md`, and `CHANGELOG.md`.
- Established clearer ownership between README overview content, OpenVMS HELP command/settings reference, build/install guidance, contributor workflow, architecture, and security policy.

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
- Preserved the separate `OVMS_AGENT_AUTO_CREATES` create-action ceiling later relied on by M281 compatibility.
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
- on promotion, move the accepted entries into a new `MNNN` section;
- identify the permanent milestone tag;
- describe behavior and compatibility changes, not every internal commit;
- include documentation/security/build changes when they affect how users or contributors operate the project;
- avoid claiming a GitHub merge SHA was VAX-run unless that exact merge SHA was actually accepted on VAX.