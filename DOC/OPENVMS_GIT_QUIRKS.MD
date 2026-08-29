# OpenVMS Git Quirks and Safe Operating Guide

This document records Git behavior actually observed while developing and accepting OVMS Agent on OpenVMS. It exists so users, contributors, and future agent work do not repeatedly rediscover the same Git/RMS traps.

These are operational lessons, not claims about every Git port or every OpenVMS filesystem. When later native evidence contradicts a rule here, update this document with the new evidence.

## Core principle

**GitHub is the repository source of truth; the OpenVMS RMS-visible working files are the native build/runtime truth.**

On the OpenVMS systems used for OVMS Agent development, those two views can temporarily disagree even when a Git command reports success. Never use a Git exit/status value alone as proof that a ref moved, a file materialized, an index changed, or a network mutation occurred.

## Authentication and network operations

Use the OVMS Agent authenticated wrapper for GitHub network operations:

```text
$ @GIT_AUTH_SETUP
$ GITAUTH FETCH ORIGIN branch-name
```

or, after login setup:

```text
$ @SYS$LOGIN:OVMS_AGENT_GIT_AUTH.COM
```

`GITAUTH` already invokes Git. Do **not** write `GITAUTH GIT FETCH ...`.

Keep `GIT_TERMINAL_PROMPT=0`; the accepted authentication design fails closed rather than falling back to interactive username/PAT prompts. Old `credential.helper=store` and askpass experiments are not the standing solution.

## A successful FETCH may populate only FETCH_HEAD

When fetching an explicit branch such as:

```text
$ GITAUTH FETCH ORIGIN M288-LANGUAGE-KNOWLEDGE-PACKS
```

native Git may report a successful fetch while no visible `origin/<branch>` remote-tracking ref is available through later commands. The fetched commit can still be present and recorded in `.git/FETCH_HEAD`.

If `GIT REV-PARSE origin/branch` is blank or otherwise untrustworthy, verify the fetched object through the RMS-visible Git control file.

On ODS-5 DCL syntax, the leading dot in `.git` must be escaped and extensionless files should be named with a trailing period:

```text
$ TYPE [.^.GIT]FETCH_HEAD.
$ TYPE [.^.GIT]HEAD.
```

Example accepted evidence:

```text
FETCH_HEAD: c9ebaed00f6aeee3141b1f5ed1ad077798908c78 ... m288-language-knowledge-packs
HEAD:       ref: refs/heads/main
```

This proves the exact fetched candidate object exists while the visible local checkout still points at `main`.

## Blank porcelain/ref output is not authoritative

On the development system, commands including these have sometimes returned completely blank output without a useful diagnostic even though Git metadata exists:

```text
$ GIT STATUS --PORCELAIN=V1 -B
$ GIT REV-PARSE HEAD
$ GIT REV-PARSE FETCH_HEAD
$ GIT BRANCH --SHOW-CURRENT
```

Do not conclude that the repository, HEAD, or fetched object is absent from blank output alone. Cross-check RMS-visible `.git` control files and GitHub branch state.

## DCL syntax for .git control files

Do not use Unix-looking `[.GIT]` for the literal `.git` directory on ODS-5. That refers to a normal `GIT` subdirectory name and can produce `%RMS-E-DNF`.

Use the escaped directory name:

```text
$ DIRECTORY [.^.GIT]
```

For extensionless Git control files, add a trailing period so DCL does not supply a default extension such as `.LIS`:

```text
$ TYPE [.^.GIT]HEAD.
$ TYPE [.^.GIT]FETCH_HEAD.
$ TYPE [.^.GIT]ORIG_HEAD.
```

This distinction is OpenVMS/DCL pathname behavior, not a Git semantic issue.

## Do not trust checkout/reset/pull to materialize RMS files

Observed native Git behavior can update repository metadata, refs, or index state without reconstructing the expected RMS-visible working file contents.

Commands such as these therefore require independent verification:

```text
git pull
git reset
git reset --mixed --no-refresh
git checkout / switch / restore
git add
```

After any branch/materialization operation, verify the actual OpenVMS files with `DIRECTORY`, `TYPE`, `SEARCH`, compiler input, or another RMS-visible check before building.

A successful Git status is not enough.

## RMS-aware restore is the established tracked-file workaround

OVMS Agent includes an RMS-aware restore path for changed tracked files. When the visible HEAD is valid and the needed candidate is represented by HEAD, use the established restore procedure rather than inventing a new checkout mechanism.

For several files, use the batch procedure:

```text
$ @GIT_RESTORE "path1,path2,path3"
```

Keep the comma-separated path list short. A long DCL command can fail with:

```text
%DCL-W-TKNOVF
```

If that occurs, assume the requested restore batch did **not** reliably materialize the files; split the list into smaller verified batches.

### Important limitation

The current `GIT_RESTORE.COM` / `GITRESTORE` path restores from Git `HEAD`; it is not a general arbitrary-SHA or `FETCH_HEAD` materializer. Do not pretend it can restore an unrelated fetched candidate when visible HEAD still points somewhere else.

When HEAD cannot safely represent the exact fetched candidate, use an explicitly audited deterministic materialization method and compare the resulting RMS-visible files against the exact GitHub candidate before native acceptance.

## Verify exact candidate identity before acceptance

For milestone work, separate these questions:

1. **Which GitHub commit is the candidate?**
2. **Which object did OpenVMS actually fetch?**
3. **Which files are physically visible to the OpenVMS compiler/runtime?**
4. **Do those visible files exactly represent the candidate tree/content?**

Do not collapse these into “Git returned success.”

Useful evidence includes:

- exact GitHub branch/PR head SHA;
- `.git/FETCH_HEAD` showing that exact SHA;
- `.git/HEAD` showing the current local ref;
- exact branch-live GitHub file content/blob SHAs;
- RMS-visible `TYPE`, `SEARCH`, `DIFFERENCES`, or hashes;
- native compile/link/runtime acceptance.

If an exact candidate SHA itself was not what OpenVMS built, say so. Distinguish **exact-SHA accepted** from **tree/content-equivalent accepted**.

## Git hashing and the RMS index

When native Git cannot hash or stage an RMS file normally, verify the physical file first.

`git hash-object` without `-w` computes a blob ID but does not store the blob. If a new blob must actually be referenced by the index/object database, use `git hash-object -w`.

Low-level operations such as `git update-index --cacheinfo` are last-resort tools. Use them only with independently verified blob SHAs and the intended repository mode.

Preserve existing executable-mode bits unless a mode change is intentional. EDT and other OpenVMS editors can contribute to local mode drift.

## Shallow clones can lie about ancestry

Before promotion/push from a shallow acceptance clone, unshallow and verify ancestry. A genuine fast-forward can appear to be a non-fast-forward rejection when the local history boundary hides the required ancestors.

## OpenVMS file versioning can preserve stale files

Repository deletion does not necessarily prove every physical OpenVMS file version is gone. When acceptance depends on a file truly disappearing (for example a compatibility shim), inspect and remove stale versions explicitly before building.

Likewise, a failed compile can leave an older object version available to LINK. Never treat a successful LINK after a failed compile as proof that the failed source produced a new object.

## Known-good decision sequence

For a fetched milestone candidate, prefer this sequence:

```text
1. Fetch with GITAUTH.
2. Verify the exact GitHub candidate SHA.
3. Verify FETCH_HEAD or the intended ref visibly contains that SHA.
4. Determine the current visible HEAD/ref separately.
5. Use the established RMS-aware restore/materialization path when applicable.
6. Verify actual RMS-visible candidate files before build.
7. Run native compile/link/regressions.
8. Record exact status and distinguish exact-SHA vs tree-equivalent acceptance.
9. Only then promote/push/tag.
```

Do not introduce a new Git mechanism merely because ordinary porcelain output is confusing if an already-validated OVMS Agent workaround covers the case.

## What belongs in this document

Add a lesson here when it is primarily about Git/OpenVMS/RMS/DCL interaction and has been reproduced or otherwise supported by native evidence.

Language/compiler-specific lessons belong in `LANGUAGE/<LANGUAGE>.MD` instead. Generic OVMS Agent defects belong in GitHub issues for `ovms-agent`.

The purpose is simple: **once we have paid the cost to learn an OpenVMS Git quirk, future users and future development sessions should start from that knowledge rather than pay for it again.**
