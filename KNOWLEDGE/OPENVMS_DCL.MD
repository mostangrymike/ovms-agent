# OpenVMS DCL knowledge

Use these as validated operating lessons, not as evidence of current symbols, logical names, files, or command status. Query the live process/system when state matters.

- DCL command procedures are record-oriented OpenVMS files. Preserve valid RMS records when editing `.COM` procedures.
- `$STATUS` can contain encoded C/POSIX child exit values. Decode the project-known POSIX exit family before applying generic native odd/even success reasoning. In this project `%X0035A401` is POSIX exit 128 failure and `%X1035A00A` is POSIX exit 1 failure; `%X00030001` is a native odd success.
- `GITAUTH` already invokes Git. Use `GITAUTH fetch ...`, not `GITAUTH GIT fetch ...`.
- On ODS-5, inspect the literal `.git` directory with `[.^.GIT]`; extensionless control files need a trailing period, such as `HEAD.` or `FETCH_HEAD.`.
- DCL logical names and symbols are different namespaces. Use `SHOW LOGICAL`/`F$TRNLNM` for logicals and `SHOW SYMBOL` for symbols rather than assuming one proves the other.
- `DEFINE/USER` is intentionally scoped to the next image activation; process-scoped test overrides require explicit restoration after the test.
- DCL command-line/token limits are real. Keep long file lists in small batches; after `%DCL-W-TKNOVF`, do not assume the intended command completed reliably.
- Use `PIPE` when shell-like stream redirection is required on this host. Ordinary Unix redirection syntax can otherwise be passed as arguments to the invoked program.
- `SET DEFAULT` changes the working directory for later relative paths. Verify the default directory before diagnosing a relative-file lookup failure.
- OpenVMS file version syntax matters: `file;*` addresses all versions. Delete or purge versions deliberately rather than assuming a single visible name represents one physical file.
