# Contributing to OVMS Agent

OVMS Agent deliberately separates repository control from native acceptance:

- **GitHub** is the repository source of truth and default editing/control plane.
- **OpenVMS V9.2-3 x86-64** is the current canonical compile/link/RMS/DCL/runtime acceptance host.

Historical DEC C/VAX compatibility remains part of the source contract where it affects external identifiers, C syntax, or linker behavior, but VAX is not the current canonical acceptance machine.

## Before starting

Read:

- `README.MD` for the overview;
- `ARCHITECTURE.MD` for subsystem boundaries;
- `BUILD.MD` for product build/acceptance behavior;
- `RULES.MD` for durable project rules;
- `DOC/OPENVMS_GIT_QUIRKS.MD` before inventing an OpenVMS Git/materialization procedure; and
- `OVMS_AGENT.HLP` when changing user-visible commands or persistent SETTINGS.

Significant work should have a GitHub issue. Milestone work normally uses an `mNNN-description` branch created from exact promoted `main`.

## Development workflow

1. Verify current promoted `main` and durable state.
2. Create/update the tracking issue.
3. Create the bounded milestone branch from exact `main`.
4. Make repository edits on GitHub by default.
5. Audit exact changed files and diff against the base.
6. For source/build/runtime work, fetch the exact candidate on OpenVMS and establish its ref/index using the validated RMS-sensitive workflow.
7. Materialize only required tracked files with `GIT_RESTORE.COM` in short verified batches when ordinary checkout/reset is unreliable.
8. Verify RMS-visible files and Git control files independently of porcelain/status output.
9. Run the applicable full build, focused regressions, and live checks.
10. Record the exact accepted candidate SHA/tree and evidence.
11. Promote through a pull request with an expected-head guard.
12. Prove whether the promoted merge tree is identical to the accepted candidate tree.
13. Create the permanent `mNNN` tag on canonical promoted `main`.
14. Apply branch-retention hygiene and update durable state only at a full green checkpoint.

Do not turn a focused milestone into unrelated cleanup unless the extra work is required for correctness or acceptance.

## GitHub-first editing

Repository discovery, committed file reads, searches, branch state, diffs, edits, issue/PR work, and history should use GitHub when the capability exists. Search results are discovery hints only; fetch the exact active-branch file before making a correctness decision or replacing a file.

After direct GitHub edits, compare the new branch head to its base and confirm no unintended files, modes, or broad formatting changes were introduced.

An OpenVMS-side edit is an exception for RMS-specific acceptance repairs or cases where the available GitHub mutation path is genuinely unsuitable. Any such edit must be pushed back to the milestone branch and re-audited on GitHub before acceptance.

## OpenVMS Git/RMS materialization

Do not assume `git pull`, `git reset`, `git checkout`, `git restore`, `git add`, `git status`, `git rev-parse`, or `$STATUS` alone proves the desired OpenVMS state. The observed Git port can update refs/index without reconstructing the RMS-visible file, or produce blank/misleading porcelain output.

Useful independent evidence includes:

```text
$ TYPE [.^.GIT]FETCH_HEAD.
$ TYPE [.^.GIT]HEAD.
$ TYPE [.^.GIT.REFS.HEADS]BRANCH-NAME.
$ DIRECTORY/FULL [.^.GIT]INDEX.
$ TYPE path-to-file
```

Use the low-level `read-tree` + `update-ref` + `symbolic-ref` sequence only in the already documented failure mode and verify each piece separately. It establishes metadata; it does not materialize working files.

For tracked files, use:

```text
$ @GIT_RESTORE "path1,path2,path3"
```

Keep each comma-separated list short enough to avoid DCL `%TKNOVF`. A successful restore must be followed by physical file verification. See `DOC/OPENVMS_GIT_QUIRKS.MD`.

Keep:

```text
core.filemode=false
```

and investigate mode drift rather than accepting it casually.

## Git network operations

Use the installed authenticated wrapper:

```text
GITAUTH
```

Install/configure it from an accepted checkout with:

```text
$ @GIT_AUTH_SETUP
```

and load it on later logins with:

```text
$ @SYS$LOGIN:OVMS_AGENT_GIT_AUTH.COM
```

`GITAUTH` already invokes Git; do not write `GITAUTH GIT ...`.

Operator-facing:

```text
$ GITAUTH --check
```

performs an authenticated, non-mutating dry-run push against the active profile/current origin to verify repository **write authorization**, not merely credential presence. Setup uses the internal/config-only check so installation itself does not require network availability.

Keep interactive Git credential prompting disabled and do not use plaintext `credential.helper=store` as the standing solution.

## OpenVMS condition-status interpretation

Do not apply the native odd/even success rule before decoding the project's POSIX-exit status range. In particular, statuses in the encoded POSIX range can be numerically odd while still representing failure. Use the established status helper semantics rather than eyeballing the low bit.

For canonical product builds, `%X00000001` remains the established successful status.

## C and linker compatibility

For DEC C/VAX-facing compatibility:

- externally visible identifiers must be 31 characters or fewer;
- implicit function declarations are not acceptable;
- preserve OpenVMS/DEC C conventions;
- keep VAX-facing C within the project's C89 discipline unless a subsystem requires otherwise;
- never rely on linker truncation.

The canonical x86-64 build is necessary acceptance evidence, but it does not waive these compatibility rules.

## Build and regression expectations

For source/build/runtime changes, normal acceptance includes:

```text
All regression tests passed.
Build completed successfully.
BUILD_STATUS = "%X00000001"
```

Focused regressions add evidence but do not silently replace a required full build.

Documentation-only work that does not feed compilation, linking, launcher/runtime behavior, or HELP parsing may use a narrower exact-diff/content validation. State explicitly why a product build is unnecessary.

For a semantic release candidate, also verify normal startup and `VERSION` on the exact candidate.

## User-facing commands and SETTINGS

`OVMS_AGENT.HLP` is part of the user interface contract. If a command or persistent setting is added, removed, renamed, gains/loses options, or materially changes behavior, update HELP in the same milestone. HELP topics must explain both what a feature does and when/why to use it.

## Documentation ownership

Update the document that owns the changed contract:

- installation/environment -> `INSTALL.MD`;
- product build/test -> `BUILD.MD`;
- contributor/workflow/rules -> `CONTRIBUTING.MD` and `RULES.MD` when durable;
- security/credentials/approval/network -> `SECURITY.MD`;
- subsystem/data flow -> `ARCHITECTURE.MD`;
- current capability summary -> `FEATURES.MD`;
- promoted user-visible history -> `CHANGELOG.MD`;
- command/SETTINGS behavior -> `OVMS_AGENT.HLP`.

Root repository filenames are uppercase by default unless an external/tooling constraint requires otherwise. Historical milestone records under `DOC/` should preserve the facts and terminology of the milestone they document; do not rewrite historical VAX evidence merely because the current canonical host is x86-64.

## Secrets

Never commit provider keys, GitHub tokens, passwords, authorization headers, `SYS$LOGIN` profile/configuration files, or logs/fixtures containing live secrets. See `SECURITY.MD`.

## Pull requests and promotion

A promotion PR should state the milestone goal, exact scope, compatibility implications, exact native acceptance evidence, and the accepted candidate SHA/tree. Use an expected-head guard when merging.

If the promoted merge SHA was not itself native-built, do not call that SHA native-green. Prove tree equivalence and say that the promoted merge **carries the exact accepted tree**.

## Milestone/release tags and branch retention

Each completed milestone receives a lightweight `mNNN` tag on canonical promoted `main`. Public semantic releases additionally receive `vX.Y.Z` on the same promoted commit.

Keep permanently:

- `main`;
- `chat-session-state`.

Keep the active milestone branch while work is in progress. After promotion, retain only the two most recently completed milestone development branches, except that any branch containing unique unpromoted work must remain until that work is resolved or otherwise preserved.

Before pruning, verify promotion, tag, issue state, tree representation on `main`, and durable SHA recording.

## Style of changes

Prefer bounded, reviewable, provider-neutral, OpenVMS-native changes with explicit compatibility/safety tradeoffs and exact evidence. The goal is not only to make OVMS Agent work, but to keep it maintainable and trustworthy on OpenVMS.
