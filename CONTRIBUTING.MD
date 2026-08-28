# Contributing to OVMS Agent

OVMS Agent is developed with an unusual but deliberate split of responsibilities:

- GitHub is the repository source of truth and development control plane.
- OpenVMS/VAX is the canonical authority for DEC C, linker, RMS, DCL, and live runtime acceptance.

Contributions should preserve that split instead of treating OpenVMS as a Unix-like checkout target.

## Before starting

Read:

- `README.MD` for the project overview.
- `ARCHITECTURE.MD` for subsystem boundaries.
- `BUILD.MD` for canonical build and regression behavior.
- `RULES.MD` for durable project-specific engineering rules.
- `OVMS_AGENT.HLP` when changing user-facing commands or persistent settings.

Significant work should have a GitHub issue before implementation. Milestone work normally uses an `mNNN-description` development branch based on the current promoted `main`.

## Development workflow

The normal workflow is:

1. Verify the current `main` SHA and durable project state.
2. Create or update the tracking issue.
3. Create a bounded milestone branch from exact `main`.
4. Make repository edits on GitHub by default.
5. Audit the exact branch diff and changed-file set.
6. Materialize only the changed tracked files on OpenVMS when VAX acceptance is required.
7. Run the applicable build/tests/live checks.
8. Preserve the exact accepted SHA and evidence.
9. Promote through a pull request with an expected-head guard.
10. Tag the promoted milestone commit and apply branch-retention hygiene.

Do not turn a focused change into a general cleanup opportunity. Separate unrelated backlog work unless it directly blocks acceptance.

## GitHub-first editing

Repository files should normally be edited through GitHub tooling. This keeps the authoritative branch state explicit and avoids unnecessary exposure to OpenVMS RMS/index edge cases during authoring.

Before replacing a whole repository file:

1. Fetch the exact active-branch content and blob SHA.
2. Change only the intended content.
3. Compare the resulting branch head against its base.
4. Confirm there are no accidental mode, formatting, or unrelated file changes.

GitHub search results are discovery hints only. Fetch the exact live file before making a correctness decision from a search result.

If the available GitHub mutation path requires replacing an impractically large file for a tiny change, a narrowly scoped VAX-side edit may be used as an exception. In that case, keep the edit deterministic, commit/push it back to the milestone branch, and re-audit the resulting exact GitHub head before acceptance.

## OpenVMS/RMS materialization

Native Git worktree/index operations on OpenVMS can report success without correctly materializing RMS files. Do not assume `git pull`, `git reset`, or ordinary checkout behavior is sufficient merely because it worked on another platform.

For accepted project workflows, use the RMS-aware restore path when changed tracked files must be materialized on VAX. For multiple files, use `GIT_RESTORE.COM` with short comma-separated path batches rather than issuing many individual restores or exceeding DCL element limits.

After materialization, verify:

```text
$ git rev-parse HEAD
$ git diff --name-only
$ git status --short
$ git config --get core.filemode
```

The normal clean state has an empty working-tree diff/status and:

```text
false
```

for `core.filemode`.

## Git network operations on OpenVMS

Use the established authenticated wrapper for GitHub network operations that require the saved OVMS Agent GitHub profile:

```text
GITAUTH
```

Install it from an accepted checkout with:

```text
$ @GIT_AUTH_SETUP
```

For future logins, load it with:

```text
$ @SYS$LOGIN:OVMS_AGENT_GIT_AUTH.COM
```

Do not introduce plaintext credential-helper storage as a convenience shortcut.

## C and linker compatibility

For VAX-facing code:

- externally visible identifiers must be 31 characters or fewer;
- implicit function declarations are not acceptable;
- preserve DEC C/OpenVMS conventions;
- avoid casually introducing modern C syntax into code that must remain VAX-compatible;
- preserve existing compiler-mode choices unless the change deliberately updates them;
- never rely on linker truncation to resolve long names.

A change that compiles only on a modern non-VAX system is not sufficient evidence for VAX-sensitive code.

## Build and regression expectations

`BUILD.COM` is authoritative. For source/build/runtime changes that can affect the product, the normal acceptance target includes:

```text
All regression tests passed.
Build completed successfully.
```

and a saved final status of:

```text
%X00000001
```

Focused regressions may add evidence but do not replace a required full build.

If a change is documentation-only and does not feed compilation/runtime, use the narrowest relevant validation and document explicitly why a full build was unnecessary.

For a semantic release candidate, also verify that normal startup and the `VERSION` command report the intended `X.Y.Z` value from the exact candidate head.

## User-facing commands and SETTINGS

`OVMS_AGENT.HLP` is part of the user interface contract.

If a user-facing command or persistent SETTINGS entry is added, removed, renamed, gains or loses options, or materially changes behavior, update the corresponding HELP topic in the same milestone.

Each HELP topic must explain both:

- what the command or setting does; and
- when or why a user would choose it.

Syntax-only documentation is not sufficient.

## Other documentation responsibilities

Update the documentation file that owns the changed contract:

- installation/environment changes -> `INSTALL.MD`;
- build/test changes -> `BUILD.MD`;
- contributor/workflow/rule changes -> `CONTRIBUTING.MD` and, when durable, `RULES.MD`;
- security boundary/credential/approval changes -> `SECURITY.MD`;
- subsystem boundaries or data-flow changes -> `ARCHITECTURE.MD`;
- user-visible milestone/release changes -> `CHANGELOG.MD`;
- command/SETTINGS changes -> `OVMS_AGENT.HLP`.

Keep `README.MD` concise enough to remain an overview and navigation page rather than duplicating every specialist document.

Root-level repository filenames are expected to be fully uppercase, including extensions, unless an external or tooling requirement makes lowercase unavoidable.

## Secrets and test data

Never commit:

- provider API keys;
- GitHub tokens;
- passwords;
- short-lived authorization headers;
- private configuration files from `SYS$LOGIN`;
- captured production secrets in logs or test fixtures.

See `SECURITY.MD` for the credential and reporting policy.

## Pull requests

A pull request should state:

- the problem or milestone goal;
- the exact scope of changed files;
- compatibility implications;
- tests/acceptance performed;
- the exact VAX-accepted feature SHA when applicable;
- whether the promotion merge itself was separately run on VAX or is only tree-equivalent to an accepted feature head.

Do not describe an untested GitHub merge SHA as VAX-green merely because its tree matches an accepted candidate. Use precise wording.

## Milestone and release tags

Each completed milestone receives a permanent lightweight `mNNN` tag on its canonical promoted `main` commit.

Public semantic releases additionally receive a `vX.Y.Z` tag on the same canonical promoted commit. From 1.0.0 onward, use semantic versioning as the release contract:

- `X.Y.Z` patch releases for backward-compatible fixes;
- `X.Y.0` minor releases for backward-compatible features;
- `X.0.0` major releases for intentional incompatible changes.

A semantic release tag does not replace the milestone tag; both identify different aspects of the same promoted checkpoint.

## Branch retention

Keep:

- `main` permanently;
- `chat-session-state` permanently;
- the active milestone branch while work is in progress;
- only the two most recently completed milestone development branches after promotion.

Before pruning an older milestone branch, verify that its accepted work is represented on `main`, its permanent tag exists, its issue is completed as appropriate, and the important SHAs are recorded durably.

## Style of changes

Prefer changes that are:

- bounded;
- reviewable;
- native to OpenVMS conventions;
- provider-neutral when the behavior is generic;
- explicit about compatibility and safety tradeoffs;
- easy to validate with exact before/after evidence.

The goal is not simply to make OVMS Agent work. The goal is to make it maintainable and trustworthy on OpenVMS.
