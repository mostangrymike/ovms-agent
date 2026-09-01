# Building OVMS Agent

`BUILD.COM` is the authoritative build and regression procedure for OVMS Agent. The project intentionally uses native OpenVMS DCL rather than requiring a Unix-oriented build system.

## Canonical acceptance environment

The current canonical product acceptance host is **OpenVMS V9.2-3 x86-64**. It is authoritative for the project's live compile/link behavior, RMS semantics, DCL behavior, regression execution, and runtime checks.

The source also preserves historical DEC C/VAX-facing compatibility constraints where they remain part of the product contract. Those compatibility constraints are not a statement that VAX is the current acceptance host.

## Normal build

From the repository root:

```text
$ @BUILD
```

A successful 1.0.0 build ends with:

```text
Building OVMS Agent Version 1.0.0...
All regression tests passed.
Build completed successfully.
Run with: $ @OVMS_AGENT
```

For milestone/release acceptance, capture the final status explicitly when requested. The established successful product-build status is:

```text
%X00000001
```

`BUILD.COM` creates/uses `[.TEST]` and `[.BUILD]`, compiles and links the product and regression images, runs the canonical regressions, bounds generated file versions, restores the caller's default directory, and returns the final OpenVMS status.

## Generated files

Generated build products live under:

```text
[.BUILD]
```

They include objects, regression images, option-file copies, logs, and the final:

```text
[.BUILD]OVMS_AGENT.EXE
```

Generated files are not source of truth. OpenVMS file versioning can leave stale generated versions, so a failed compile followed by a successful link is not proof that the failed source produced a fresh object.

## Quiet and verbose builds

Normal compiler/linker detail is captured in:

```text
[.BUILD]OVMS_BUILD.LOG
```

For verbose output:

```text
$ DEFINE/PROCESS OVMS_AGENT_BUILD_VERBOSE YES
$ @BUILD
$ DEASSIGN/PROCESS OVMS_AGENT_BUILD_VERBOSE
```

Recognized true values include `YES`, `TRUE`, `1`, and `ON`.

## Build structure

The top-level procedure delegates coherent module groups to:

- `BUILD_LLM_MODULES.COM`;
- `BUILD_COMMAND_MODULES.COM`;
- `LLM_MODULES.OPT`;
- `COMMAND_MODULES.OPT`.

`BUILD.COM` builds the remaining core/project modules, links the final image, builds focused regression executables, and runs the canonical sequence. Do not add the same object through both a top-level link command and an options file.

## Compiler compatibility

The repository contains modules compiled with different historical compiler modes, including C99 and `RELAXED_ANSI89`, according to subsystem needs. Preserve existing mode choices unless the change deliberately updates them.

For DEC C/VAX-facing compatibility:

- externally visible identifiers must be 31 characters or fewer;
- implicit function declarations are not acceptable;
- preserve OpenVMS/DEC C calling and library conventions;
- keep VAX-facing C within the project's C89/DEC C discipline unless an existing subsystem specifically requires otherwise;
- never rely on compiler/linker name truncation.

The canonical x86-64 build does not remove these compatibility requirements.

## Regression expectations

A normal `@BUILD` is both a build and the canonical regression suite. Linking the final executable alone is not acceptance.

Focused tests may supplement the full build and should have their final OpenVMS status captured when used as evidence. They do not replace a required full build for source/build/runtime changes.

Milestone-specific diagnostic hooks such as historical failure-injection arguments are test paths, not normal installation or acceptance commands.

## When a full build is required

Run the full `@BUILD` for changes that can affect:

- C source or headers;
- command/LLM module composition;
- options/linker inputs;
- launcher/build DCL procedures;
- runtime behavior;
- compatibility-shim removal; or
- regression behavior.

A documentation-only change that does not feed compilation, linking, launcher behavior, HELP-library syntax, or runtime may use a narrower documentation validation gate. The milestone record must state why a product rebuild was unnecessary.

## Exact candidate versus tree-equivalent promotion

Native acceptance belongs to the exact candidate that was actually materialized and tested. After promotion, a GitHub merge commit may be treated as carrying the accepted product tree only when its tree is proven identical to the accepted candidate tree.

Do not say a merge SHA itself was native-built when only its tree-equivalent parent/candidate was built. Record both facts precisely.

## OpenVMS/RMS acceptance workflow

GitHub is the repository control plane; the canonical OpenVMS host is the native acceptance authority. Because the observed OpenVMS Git port is RMS-sensitive, repository identity, Git metadata, physical files, and native execution must be verified separately.

For source/build/runtime milestones:

1. Create/audit the exact candidate on GitHub.
2. Fetch it on OpenVMS with `GITAUTH` when authenticated network access is required.
3. Verify the exact fetched SHA through RMS-visible Git control files when normal porcelain/ref output is unreliable (for example `[.^.GIT]FETCH_HEAD.` and the intended branch-ref file).
4. Establish local index/ref metadata using the already validated workflow when ordinary checkout/reset is unreliable.
5. Materialize changed tracked files through the RMS-aware restore path in short verified batches.
6. Verify physical RMS-visible files, not Git status alone.
7. Run the required full build/focused tests/live checks.
8. Capture final OpenVMS statuses.
9. Confirm `core.filemode=false` and investigate any local working-tree anomalies rather than assuming porcelain output is authoritative.
10. Promote only after the acceptance evidence is complete.

See `DOC/OPENVMS_GIT_QUIRKS.MD` for the detailed operating guide and `CONTRIBUTING.MD` for milestone workflow.

## Build failures

When a build fails:

1. Preserve the exact diagnostic and status.
2. Inspect `[.BUILD]OVMS_BUILD.LOG` when needed.
3. Fix the smallest coherent cause.
4. Run the relevant focused regression when one exists.
5. Re-run the full build whenever the changed surface requires it.

OVMS Agent can capture failed project-build evidence for repair workflows, but this repository's `BUILD.COM` remains authoritative for building OVMS Agent itself.
