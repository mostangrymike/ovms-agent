# OpenVMS Git Quirks and Safe Operating Guide

This document is the **current operational guide** for Git behavior actually observed while developing and accepting OVMS Agent on OpenVMS. It is the one living operating document under `DOC/`; the milestone reconciliation/parity files are historical records.

Current canonical native acceptance host: **OpenVMS V9.2-3 x86-64**.

These are observed operating lessons, not claims about every OpenVMS Git port or filesystem. Update the guide when later native evidence supersedes a lesson.

## Core principle

**GitHub branch/tree state is repository truth; RMS-visible OpenVMS working files are native compiler/runtime input truth. Git metadata is a third state that must sometimes be verified independently.**

Never use one Git command's output or `$STATUS` alone as proof that a ref moved, an index changed, a working file materialized, or a network mutation occurred.

## Authentication: use GITAUTH

Use the installed authenticated wrapper for GitHub network operations:

```text
$ @GIT_AUTH_SETUP
$ GITAUTH fetch origin branch-name
```

or on later logins:

```text
$ @SYS$LOGIN:OVMS_AGENT_GIT_AUTH.COM
```

`GITAUTH` already invokes Git. Do not write `GITAUTH GIT ...`.

Keep `GIT_TERMINAL_PROMPT=0`; the accepted design fails closed instead of falling back to interactive username/PAT prompts. Plaintext `credential.helper=store`, the former global helper experiment, and askpass are not the standing solution.

### GITAUTH --check means write authorization

Operator-facing:

```text
$ GITAUTH --check
```

performs an authenticated, non-mutating dry-run push to the active profile's **current origin**. It verifies repository write authorization, not merely that profile fields are populated.

This catches two historical failure classes:

- a token that can authenticate but lacks write access to the repository; and
- a stale/wrong active profile whose repository-scoped auth does not match current origin.

The preflight uses a temporary target ref with dry-run semantics; no remote ref should be created. Verify that fact independently when it is part of acceptance.

`GIT_AUTH_SETUP.COM` uses the internal/config-only check so wrapper installation does not require current network availability.

## Decode POSIX exit statuses before native odd/even reasoning

The project can receive POSIX-exit values encoded into OpenVMS status space. Do **not** apply the generic native odd/even success rule first.

Current constants:

```text
OVMS_POSIX_EXIT_BASE = 0x0035A000UL
OVMS_POSIX_EXIT_MASK = 0x0FFFF800UL
```

Known examples:

- `%X0035A401` = encoded POSIX exit 128 = **failure**, despite being odd;
- `%X1035A00A` = encoded POSIX exit 1 = **failure**;
- `%X00030001` = native odd success;
- `%X0F0180EA` = curl timeout / failure.

`ovms_status_propagate` maps POSIX nonzero to native even failure status 2. Never call `%X0035A401` success.

## Explicit fetch may be proven through FETCH_HEAD

An explicit fetch can succeed even when later porcelain/remote-tracking-ref output is blank or unhelpful. Verify the exact fetched object through the RMS-visible control file:

```text
$ TYPE [.^.GIT]FETCH_HEAD.
```

On ODS-5 DCL, the literal `.git` directory is escaped as `[.^.GIT]`; extensionless control files need a trailing period.

Also useful:

```text
$ TYPE [.^.GIT]HEAD.
$ TYPE [.^.GIT.REFS.HEADS]BRANCH-NAME.
$ DIRECTORY/FULL [.^.GIT]INDEX.
```

## Blank porcelain/ref output is not authoritative

Observed commands such as `git status`, `rev-parse`, and branch display have sometimes produced blank/misleading output even though Git metadata exists. Do not conclude the repository/ref is absent from blank output alone; cross-check GitHub and RMS-visible `.git` files.

Likewise, a `.GIT.DIR` directory by itself is **not proof of a valid Git repository**. Historical sandboxes showed `.GIT.DIR` without a valid `HEAD`/refs structure. A fresh valid OpenVMS repository contained an RMS-visible `HEAD.` such as:

```text
ref: refs/heads/master
```

and native `git rev-parse --is-inside-work-tree` succeeded.

## Reset/checkout/pull can fail to materialize RMS files

Observed native Git behavior can update refs or index state without reconstructing expected RMS-visible working-file contents. Treat these as requiring independent physical verification:

```text
git pull
git reset
git checkout / switch / restore
git add
```

Verify actual files with `TYPE`, `DIRECTORY`, `SEARCH`, hashes/comparisons, compiler input, or another RMS-visible check before building.

## Validated low-level candidate-establishment path

When exact `FETCH_HEAD` is known and ordinary branch establishment is unreliable, the validated recovery path is:

```text
$ GIT "read-tree" "FETCH_HEAD"
$ GIT "update-ref" "refs/heads/<candidate-branch>" "FETCH_HEAD"
$ GIT "symbolic-ref" "HEAD" "refs/heads/<candidate-branch>"

$ DIRECTORY/FULL [.^.GIT]INDEX.
$ TYPE [.^.GIT]HEAD.
$ TYPE [.^.GIT.REFS.HEADS]<CANDIDATE-BRANCH>.
```

Treat each as a separate proof:

- `read-tree` establishes index content;
- `update-ref` establishes the branch ref;
- `symbolic-ref` establishes HEAD;
- **none of them materializes the working files**.

Do not invent equivalent low-level sequences when the established workflow already covers the observed failure mode.

## RMS-aware tracked-file restore

When HEAD represents the intended candidate, use the established batch restore:

```text
$ @GIT_RESTORE "path1,path2,path3"
```

Keep the list short enough to avoid `%DCL-W-TKNOVF`. If token overflow occurs, assume the batch did not reliably restore the requested files; split it and verify each smaller batch.

`GIT_RESTORE.COM` restores from current Git HEAD. It is not a general arbitrary-SHA/FETCH_HEAD materializer.

## Selective restore can leave a hybrid tree

Restoring only files changed by a candidate does not prove every other working file matches that candidate. M288 acceptance exposed a stale earlier-branch regression file outside the M288 changed-file list; the full regression suite caught the hybrid tree.

Therefore:

- know what local modifications/fixtures existed before materialization;
- restore suspect tracked paths from current exact HEAD;
- do not discard unrelated local modifications without assessment; and
- use the full native build/regression suite to expose hidden contamination when product inputs changed.

## Git status implementation and scratch-file filtering

M290 moved Git status/diff handling toward RMS-safe direct comparison and correct native/POSIX status interpretation.

M291 fixed a self-pollution bug where status scratch files created by OVMS Agent could appear as untracked. The current filter removes only exact untracked (`?? `) basenames:

```text
OVMS_AGENT_GIT_META.TMP
OVMS_AGENT_GIT_INDEX.TMP
OVMS_AGENT_GIT_CACHED.TMP
OVMS_AGENT_GIT_OTHER.TMP
OVMS_AGENT_GIT_HASH.TMP
```

Tracked files with the same name and lookalikes such as `.TMP.BAK` are not hidden.

Do not generalize this into “ignore all TMP files”; the filter is deliberately exact.

## Exact candidate identity versus tree-equivalent promotion

Separate these questions:

1. Which GitHub commit/tree is the candidate?
2. Which object did OpenVMS fetch?
3. Which refs/index does local Git metadata represent?
4. Which files are physically visible to compiler/runtime?
5. Which exact SHA was native-tested?
6. Does the promoted merge have an identical tree?

If the merge SHA itself was not native-built, do not call it native-green. It may be accepted as a **tree-equivalent promotion** only after proving its tree equals the exact native-accepted candidate tree.

## Hashing, index operations, and file modes

When native Git cannot hash/stage an RMS file normally, verify the physical file first.

`git hash-object` without `-w` computes a blob ID but does not store it. Use `-w` only when the object must actually be referenced. Low-level `update-index --cacheinfo` is a last-resort operation and requires independently verified blob SHA and repository mode.

Keep:

```text
core.filemode=false
```

and preserve repository executable bits unless an intentional change requires otherwise. OpenVMS editors can cause meaningless local mode drift.

## Shallow history

Before promotion/push from a shallow clone, unshallow and verify ancestry. A genuine fast-forward can otherwise appear locally as a non-fast-forward rejection.

## OpenVMS file versions and stale objects

Repository deletion does not prove all physical OpenVMS file versions are gone. Verify stale versions explicitly when acceptance depends on deletion.

A failed compile can leave an older object version available to LINK; a successful LINK after a failed compile is not proof of a fresh object.

## Known-good candidate sequence

1. Verify the exact GitHub candidate SHA/tree.
2. Fetch with `GITAUTH`.
3. Verify RMS-visible `FETCH_HEAD.` (or intended ref) contains that SHA.
4. Establish index/ref metadata with the validated workflow if normal checkout/reset is unreliable.
5. Verify `INDEX.`, `HEAD.`, and the branch-ref file separately when relevant.
6. Materialize changed tracked files with `GIT_RESTORE` in short verified batches.
7. Check for stale prior-branch/local tracked modifications outside the candidate changed-file list.
8. Verify RMS-visible candidate files before build.
9. Run native compile/link/regressions/live checks appropriate to the changed surface.
10. Record exact native status/evidence.
11. Promote only after acceptance.
12. Compare accepted candidate tree to promoted merge tree before using tree-equivalent promotion language.
13. Tag/prune refs only after promotion evidence is durable.

## What belongs here

Add lessons here when they are primarily about Git/OpenVMS/RMS/DCL interaction and supported by native evidence. Language/compiler-specific lessons belong under `LANGUAGE/`; generic product defects belong in GitHub issues.

The purpose is simple: once a Git/RMS quirk has been proven, future users and development sessions should start from that evidence rather than rediscover it.
