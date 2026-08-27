# M275 Backlog Hardening Reconciliation

## Scope

M275 is the post-M274 hardening milestone tracked by issue #88. It starts from VAX-green `main` at `1fa959e0f577d220c0c17bde0e39ea904c90fb6a` and addresses the highest-leverage deterministic defects exposed by the M274 stress test while removing the completed `m274_netcfg` workload fixture from the active source tree.

The final VAX-green M275 code checkpoint is `337233fd389f5d8723ff98fe19d6a1ff4606f25a` on `m275-backlog-hardening`. At that exact head, canonical `@BUILD` printed `All regression tests passed.`, returned `%X00000001`, and left the tracked worktree clean. The newly published Agent image then passed live production-path acceptance for #83, #84, and #86.

This document is intentionally a reconciliation-only change. Its commit does not supersede `337233fd389f5d8723ff98fe19d6a1ff4606f25a` as the VAX-tested M275 code checkpoint unless the reconciliation head itself is separately accepted on VAX.

## M274 workload fixture cleanup

M274 used the tracked `m274_netcfg` project as a real self-hosting stress workload. M275 removes the ten tracked fixture files from the active source tree while preserving Git history and `DOC/M274_RECONCILIATION.MD` as the permanent record of the stress test.

The removed fixture files are:

- `m274_netcfg/build.com`
- `m274_netcfg/diff.c`
- `m274_netcfg/diff.h`
- `m274_netcfg/fileutil.c`
- `m274_netcfg/fileutil.h`
- `m274_netcfg/main.c`
- `m274_netcfg/rules.c`
- `m274_netcfg/rules.h`
- `m274_netcfg/storage.c`
- `m274_netcfg/storage.h`

The OpenVMS working tree was also cleaned of stale physical fixture versions before final acceptance.

## #83 — live AGENT/TOOL/RUN arguments are no longer transcript-sanitized or truncated

M274 exposed two related production-path defects in `AGENT/TOOL/RUN`: live tool arguments were bounded by the smaller transcript buffer, and transcript sanitization mutated literal pipe characters and other control characters before dispatch.

M275 separates the live dispatch argument path from bounded transcript storage. A dedicated regression exercises a payload larger than the former 768-byte boundary, verifies literal `||` and TAB preservation, and verifies explicit rejection when a caller-provided destination buffer is genuinely too small.

After the canonical green build at `337233fd389f5d8723ff98fe19d6a1ff4606f25a`, the newly published Agent image passed a live production-path check:

`AGENT/TOOL/RUN GREP "||" SRC/GIT_RMS_RESTORE.C /LIMIT=5 /CASE=SENSITIVE`

The tool returned source lines containing literal `||`, proving the production wrapper no longer converts pipes to spaces before dispatch. Agent exit status was `%X00000001` and the worktree remained clean. Issue #83 was closed as completed.

## #84 — nested GITRESTORE now separates local RMS paths from canonical Git tree paths

M274 showed that `GITRESTORE` conflated two namespaces: a local current-directory RMS path such as `main.c` and the repository-relative Git tree path required by `HEAD:<path>`.

The M275 investigation isolated several OpenVMS-specific behaviors on the VAX Git port:

- from `[.TEST]`, `git rev-parse --show-prefix` reports lowercase `test/`;
- Git stores the tracked path as `TEST/GAP008_RMS.TXT` and tree lookup is case-sensitive;
- `git ls-tree --name-only` did not provide usable names on this port;
- plain `git ls-files --full-name` from the nested directory was filtered through the lowercase cwd spelling and returned no entries;
- `git ls-files --full-name -- ":(top,icase)test/GAP008_RMS.TXT"` correctly returned the exact stored spelling `TEST/GAP008_RMS.TXT`.

The accepted implementation keeps the local RMS filename separate, builds the combined repository candidate from the current prefix plus local path, validates the candidate, and resolves it through a top-level case-insensitive `ls-files` pathspec. It requires one non-empty safe returned path and then uses Git's exact stored spelling for `cat-file` while writing the original local RMS path.

Live production-path acceptance ran from `SYS$SYSDEVICE:[MIKE.OVMS_AGENT.TEST]`:

`@[-]GIT_RESTORE "GAP008_RMS.TXT"`

The newly built Agent reported `Git HEAD contents restored and verified as the effective OpenVMS file version.`, returned `%X00000001`, and left `git status --short` empty. Issue #84 was closed as completed.

## #86 — OpenVMS checkout filemode normalization

M274 also showed that successful RMS materialization could leave a mode-only Git diff because ordinary OpenVMS files were observed as executable while the Git index stored mode `100644`.

M275 updates `GIT_AUTH_SETUP.COM` to normalize repository-local `core.filemode=false` for the OpenVMS checkout. This does not change GitHub content or index modes; it tells the local Git worktree comparison to ignore an executable-bit distinction that is not meaningful for this RMS materialization path.

Final live acceptance on `337233fd389f5d8723ff98fe19d6a1ff4606f25a` forced `core.filemode=true`, ran `@GIT_AUTH_SETUP`, observed procedure status `%X00000001`, then verified `git config --get core.filemode` returned `false`. Exact HEAD was unchanged and `git status --short` remained empty. Issue #86 was closed as completed.

## Regression and build integration

M275 adds a dedicated backlog regression and integrates it into the canonical build. The branch also preserves the existing M265 RMS-aware restore regression while adding the M275-specific raw argument and nested restore checks.

The final code checkpoint `337233fd389f5d8723ff98fe19d6a1ff4606f25a` is eight commits ahead and zero behind the M274-promoted `main` base. Relative to that base, the active hardening changes touch the M275 build/test wiring, Git authentication setup, RMS-aware restore implementation, transcript/tool argument handling, and the intended M274 fixture deletions.

Canonical final code acceptance on VAX:

- exact head `337233fd389f5d8723ff98fe19d6a1ff4606f25a`;
- `git diff --check main..HEAD` clean;
- `@BUILD` printed `All regression tests passed.`;
- final build status `%X00000001`;
- live #83 production dispatch status `%X00000001`;
- live nested #84 restore status `%X00000001`;
- live #86 setup normalization status `%X00000001`;
- final tracked worktree clean.

## Remaining backlog

M275 deliberately does not absorb unrelated unresolved M274 findings. Significant open backlog remains tracked separately, including #58, #68, #78, #79, #81, and #82. Promotion of M275 does not imply those issues are resolved.

## Promotion state

At the time this reconciliation document was added, `337233fd389f5d8723ff98fe19d6a1ff4606f25a` is the accepted M275 code checkpoint and `main` remains at M274 promotion commit `1fa959e0f577d220c0c17bde0e39ea904c90fb6a`.

The reconciliation-only branch head must receive the normal final VAX gate before the M275 promotion PR is opened. After promotion, create permanent lightweight tag `m275` on the canonical promoted `main` commit and apply the milestone branch-retention rule from `RULES.md`.