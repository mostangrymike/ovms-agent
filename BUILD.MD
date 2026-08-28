# Building OVMS Agent

`BUILD.COM` is the authoritative build and regression procedure for OVMS Agent. The project intentionally keeps its build logic in native OpenVMS DCL rather than requiring a Unix-oriented build system.

## Normal build

From the repository root:

```text
$ @BUILD
```

The procedure determines the project directory from its own filespec, creates `[.TEST]` and `[.BUILD]` when needed, bounds old generated file versions in `[.BUILD]`, compiles and links the program and its supporting test images, runs the regression suite, restores the caller's default directory, and returns the final OpenVMS status.

A successful 1.0.0 build ends with:

```text
Building OVMS Agent Version 1.0.0...
All regression tests passed.
Build completed successfully.
Run with: $ @OVMS_AGENT
```

For milestone and release acceptance, save and display the final status explicitly when requested. The established successful build status is:

```text
%X00000001
```

## Generated files

Build products live under:

```text
[.BUILD]
```

That directory contains object files, test images, linker option copies, the final `OVMS_AGENT.EXE`, and build diagnostics. These are generated artifacts and should not be treated as source of truth.

Before a normal build, `BUILD.COM` purges generated `[.BUILD]` files to keep one version. This bounds normal OpenVMS file-version accumulation while preserving the newest generated artifact.

## Quiet and verbose builds

Normal builds redirect compiler/linker detail to:

```text
[.BUILD]OVMS_BUILD.LOG
```

To request verbose build output, define the process logical `OVMS_AGENT_BUILD_VERBOSE` to one of the recognized true values before invoking `@BUILD`, for example:

```text
$ DEFINE/PROCESS OVMS_AGENT_BUILD_VERBOSE YES
$ @BUILD
$ DEASSIGN/PROCESS OVMS_AGENT_BUILD_VERBOSE
```

Recognized true forms include `YES`, `TRUE`, `1`, and `ON`.

Verbose output is useful when diagnosing a compiler, linker, or regression failure. Routine milestone acceptance normally uses the standard build output plus the captured log when additional detail is necessary.

## Build structure

The top-level procedure delegates coherent module families to supporting DCL procedures and options files:

- `BUILD_LLM_MODULES.COM` builds the provider-neutral LLM/agent subsystem and related support modules.
- `BUILD_COMMAND_MODULES.COM` builds the command subsystem.
- `LLM_MODULES.OPT` supplies the LLM-side object set to the linker.
- `COMMAND_MODULES.OPT` supplies the command-side object set to the linker.
- `BUILD.COM` builds the remaining core/project modules, links the final image, builds focused regression executables, and runs the canonical regression sequence.

The final executable is linked as:

```text
[.BUILD]OVMS_AGENT.EXE
```

Do not add the same object through both the top-level link command and an options file. Several build comments explicitly protect against duplicate linkage of shared transaction/RMS objects.

## Compiler compatibility

The repository includes code compiled with both C99 and `RELAXED_ANSI89` modes depending on the module and historical compatibility requirements.

For VAX-facing work, project rules remain stricter than simply accepting whatever a modern compiler allows:

- do not introduce implicit function declarations;
- preserve DEC C/OpenVMS calling and library conventions;
- treat the 31-character externally visible identifier limit as a hard compatibility constraint;
- avoid modern C syntax in VAX-sensitive code unless the live subsystem already requires it;
- never rely on compiler/linker identifier truncation.

When adding a new external function, global variable, or linker-visible symbol, count the identifier before asking VAX to discover the problem for you.

## Tests

A normal `@BUILD` is not merely compilation. It builds and executes the project's canonical regression programs. A source change is not accepted merely because the final executable linked.

When a focused regression procedure or executable is used as additional milestone evidence, capture its final OpenVMS status separately. Focused tests supplement the normal build; they do not silently replace it when the changed surface can affect normal compilation, linking, or runtime behavior.

## Intentional diagnostic path

`BUILD.COM` contains test-only diagnostic hooks used by regression work. Do not use milestone-specific diagnostic arguments during a normal production build unless the corresponding test explicitly requires them.

For example, the `M251_DIAG_FAIL` argument intentionally exercises a failed-build path. It is not an installation or acceptance command.

## When a full build is required

Run the full `@BUILD` for changes that can affect:

- C source or headers;
- command/LLM module build lists;
- options files or linker composition;
- DCL procedures used by the build or launcher;
- compatibility shim removal;
- runtime behavior whose acceptance depends on the built image.

A documentation-only change that does not feed the build or runtime may use a narrower documentation/RMS validation gate, but the milestone record must say why a full product build was not required.

## VAX/OpenVMS acceptance

GitHub is the repository control plane, but OpenVMS/VAX remains the canonical authority for DEC C behavior, linker behavior, RMS semantics, DCL semantics, and live runtime acceptance.

The accepted workflow is:

1. Make and audit the repository change on GitHub.
2. Fetch the exact candidate ref on OpenVMS.
3. Materialize changed tracked files with the RMS-aware restore path when native Git checkout/reset is unreliable.
4. Verify the exact candidate SHA and a clean intended diff.
5. Run the required build/tests.
6. Save the final status.
7. Verify `git status --short` and `git diff --name-only` are empty and `core.filemode=false`.
8. Promote only after the acceptance evidence is complete.

For a release candidate, also verify the runtime `VERSION` command and normal startup banner report the intended semantic version before promotion.

See `CONTRIBUTING.MD` for the full milestone workflow.

## Build failures

When a build fails:

1. Preserve the exact compiler/linker/test diagnostic and final status.
2. Inspect `[.BUILD]OVMS_BUILD.LOG` when the standard output is intentionally quiet.
3. Fix the smallest coherent cause rather than broadening the change opportunistically.
4. Re-run the relevant focused test if one exists.
5. Re-run the full build when the changed surface requires it.

OVMS Agent itself can capture failed project build evidence for repair workflows, but the repository's own `BUILD.COM` remains authoritative for building OVMS Agent.
