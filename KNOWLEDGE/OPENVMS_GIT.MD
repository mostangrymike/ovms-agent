# OpenVMS Git knowledge

Use these as validated operating lessons, not as evidence that any current repository state exists. Verify the live ref, index, and RMS-visible files independently.

- Use `GITAUTH` for authenticated GitHub network operations. It already invokes Git; do not write `GITAUTH GIT ...`.
- Keep `GIT_TERMINAL_PROMPT=0` and do not expose credentials.
- After an explicit fetch, verify the fetched object through RMS-visible `.git/FETCH_HEAD`; blank or misleading porcelain output is not authoritative on the current OpenVMS Git port.
- On ODS-5 DCL, the literal `.git` directory is inspected as `[.^.GIT]`, and extensionless control files need a trailing period, for example `TYPE [.^.GIT]FETCH_HEAD.`.
- Ref or index movement does not prove working files were materialized. `read-tree`, `update-ref`, and `symbolic-ref` establish different pieces of Git metadata; none reconstructs RMS-visible working files.
- `GIT_RESTORE.COM` is the established RMS-aware tracked-file restore path when HEAD is already the intended candidate. Keep batches short enough to avoid DCL token overflow.
- Selective restore can leave a hybrid tree. Assess local modifications first, restore suspect tracked paths from exact HEAD, and use native build/regressions when product inputs changed.
- Direct Git reads of valid RMS variable-record files can fail with `short read while indexing`. When this known behavior appears, prefer the accepted RMS-safe OVMS Agent Git status/context path or stream text through `TYPE` for content hashing.
- Keep `core.filemode=false`; preserve repository executable bits unless an intentional change requires otherwise.
- A failed compile can leave a stale older object that LINK may still consume. Successful linking after failed compilation is not proof of a fresh object.
- Separate exact candidate SHA, tree identity, fetched object, local refs/index, RMS-visible file contents, exact native-tested SHA, and promoted merge identity. A tree-equivalent promoted merge is not itself native-green unless that exact SHA was built.
- Before pruning a branch, prove it has no unique unpromoted work and retain the permanent milestone tag.
