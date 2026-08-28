# Contributing to OVMS Agent

OVMS Agent is developed with an unusual but deliberate split of responsibilities:

- GitHub is the repository source of truth and development control plane.
- OpenVMS/VAX is the canonical authority for DEC C, linker, RMS, DCL, and live runtime acceptance.

Contributions should preserve that split instead of treating OpenVMS as a Unix-like checkout target.

## Before starting

Read:

- `README.md` for the project overview.
- `ARCHITECTURE.md` for subsystem boundaries.
- `BUILD.md` for canonical build and regression behavior.
- `RULES.md` for durable project-specific engineering rules.
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

## User-facing commands and SETTINGS

`OVMS_AGENT.HLP` is part of the user interface contract.

If a user-facing command or persistent SETTINGS entry is added, removed, renamed, gains or loses options, or materially changes behavior, update the corresponding HELP topic in the same milestone.

Each HELP topic must explain both:

- what the command or setting does; and
- when or why a user would choose it.

Syntax-only documentation is not sufficient.

## Other documentation responsibilities

Update the documentation file that owns the changed contract:

- installation/environment changes -> `INSTALL.md`;
- build/test changes -> `BUILD.md`;
- contributor/workflow/rule changes -> `CONTRIBUTING.md` and, when durable, `RULES.md`;
- security boundary/credential/approval changes -> `SECURITY.md`;
- subsystem boundaries or data-flow changes -> `ARCHITECTURE.md`;
- user-visible milestone/release changes -> `CHANGELOG.md`;
- command/SETTINGS changes -> `OVMS_AGENT.HLP`.

Keep `README.md` concise enough to remain an overview and navigation page rather than duplicating every specialist document.

## Secrets and test data

Never commit:

- provider API keys;
- GitHub tokens;
- passwords;
- short-lived authorization headers;
- private configuration files from `SYS$LOGIN`;
- captured production secrets in logs or test fixtures.

See `SECURITY.md` for the credential and reporting policy.

## Pull requests

A pull request should state:

- the problem or milestone goal;
- the exact scope of changed files;
- compatibility implications;
- tests/acceptance performed;
- the exact VAX-accepted feature SHA when applicable;
- whether the promotion merge itself was separately run on VAX or is only tree-equivalent to an accepted feature head.

Do not describe an untested GitHub merge SHA as VAX-green merely because its tree matches an accepted candidate. Use precise wording.

## Milestone tags and branch retention

Each completed milestone receives a permanent lightweight `mNNN` tag on its canonical promoted `main` commit.

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