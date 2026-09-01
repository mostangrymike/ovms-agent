# OVMS Agent

OVMS Agent is a native agentic programming assistant for OpenVMS. It brings modern AI-assisted software-development workflows to OpenVMS while respecting DCL, RMS file semantics, native compiler/linker behavior, explicit approval boundaries, and OpenVMS Git quirks.

## Release and acceptance status

- **Current public release:** `1.0.0`
- **Latest promoted milestone:** `M292`
- **Canonical native acceptance host:** OpenVMS V9.2-3 x86-64
- **Primary build procedure:** `BUILD.COM`
- **Native command reference:** `OVMS_AGENT.HLP`
- **License:** MIT

Historical VAX/DEC C compatibility remains an intentional source constraint where it affects external identifiers, C89-era syntax, or linker behavior; it is not the current canonical acceptance host.

## Documentation

| File | Purpose |
| --- | --- |
| [`INSTALL.MD`](INSTALL.MD) | Prerequisites, installation, first run, provider setup, and authenticated Git setup. |
| [`BUILD.MD`](BUILD.MD) | Authoritative product build, regressions, diagnostics, and native acceptance workflow. |
| [`CONTRIBUTING.MD`](CONTRIBUTING.MD) | GitHub-first development, RMS-aware OpenVMS acceptance, compatibility rules, and milestone hygiene. |
| [`SECURITY.MD`](SECURITY.MD) | Credentials, approval boundaries, provider/GitHub transport, DCL/write/network safety, and reporting. |
| [`ARCHITECTURE.MD`](ARCHITECTURE.MD) | Subsystem boundaries, persistence, guarded writes, provider flow, native toolchain execution, and Git/GitHub integration. |
| [`FEATURES.MD`](FEATURES.MD) | Plain-language catalog of current capabilities. |
| [`CHANGELOG.MD`](CHANGELOG.MD) | Promoted milestone/release history and current unreleased work. |
| [`OVMS_AGENT.HLP`](OVMS_AGENT.HLP) | Complete OpenVMS HELP source for commands and persistent SETTINGS. |
| [`RULES.MD`](RULES.MD) | Durable project-development and acceptance rules. |
| [`DOC/`](DOC/) | Historical reconciliation/parity records plus the current OpenVMS Git/RMS operating guide. |
| [`LANGUAGE/`](LANGUAGE/) | Native-validated language knowledge packs and build profiles. |

## Major capabilities

OVMS Agent currently provides:

- project/tree/file inspection and search;
- ASK, CHAT, REVIEW, read-only agent analysis, planning, and persistent sessions;
- guarded file creation and modification with OpenVMS RMS-aware behavior;
- atomic multi-hunk `structured_patch` editing for ordinary `AGENT/WRITE` existing-file changes;
- transactional multi-file change sets and rollback-oriented repair workflows;
- project builds and captured build diagnostics;
- guarded native `build_source` execution using language-specific toolchain profiles;
- Git-aware project context and RMS-safe status/diff handling;
- guarded GitHub operations and prompt-free authenticated raw Git through `GITAUTH`;
- persistent provider, GitHub, and ordinary SETTINGS configuration;
- provider-neutral support for direct providers and OpenAI-compatible intermediaries such as Requesty;
- `PROVIDER TEST` / `PROVIDER TEST name` response-health diagnostics;
- bounded autonomous execution and AUTOPILOT safety controls;
- image input and external/MCP tool integration under approval controls; and
- native-validated language guidance.

The shared knowledge-pack layer currently ships curated COBOL and Fortran `.MD` packs. Separately, guarded `build_source` toolchain profiles are shipped for MACRO32, Fortran, COBOL, Pascal, C++, BASIC, Java, Python, Perl, and DCL. A project's own build procedure remains authoritative when one exists.

## Requirements

You need OpenVMS, a suitable C compiler, TCP/IP/HTTPS for network-backed features, and preferably Git. For the VSI Git environment used during development, HTTPS Git may require:

```text
$ SET PROCESS/PARSE_STYLE=EXTENDED
$ SET PROCESS/PRIVILEGE=SHARE
```

`SHARE` must already be authorized for the account.

See `INSTALL.MD` and `BUILD.MD` for setup and build details.

## Build and run

From the project root:

```text
$ @BUILD
```

A successful 1.0.0 product build ends with:

```text
Building OVMS Agent Version 1.0.0...
All regression tests passed.
Build completed successfully.
Run with: $ @OVMS_AGENT
```

Start the program with:

```text
$ @OVMS_AGENT
```

Useful first commands include:

```text
HELP
SETTINGS
ROOT
TREE
STATUS
PROVIDER LIST
PROVIDER SHOW
PROVIDER TEST
MODEL
ASK Explain this project in plain English.
AGENT Explain the architecture of this project. Do not modify anything.
AGENT/LANG
AGENT/GITHUB/CHECK
```

Exit with `QUIT` or `EXIT`.

## Provider configuration and health testing

Named provider profiles are stored outside the repository in:

```text
SYS$LOGIN:OVMS_AGENT_CONFIG.DAT
```

Useful commands are:

```text
PROVIDER LIST
PROVIDER SHOW [name]
PROVIDER TEST [name]
PROVIDER USE name
PROVIDER ADD name
PROVIDER DELETE name
MODEL [model-name]
```

`PROVIDER TEST` sends one bounded plain-text no-op request through each configured profile; `PROVIDER TEST name` tests one profile. It does not change the active profile. Responsive profiles report `OK`; failures are classified as `CONFIG`, `LOCAL ERROR`, `TRANSPORT`, `API ERROR`, `EMPTY`, or `INVALID`. The test uses real provider transport and may consume a very small amount of provider usage. It is a basic response-health check, not a tool-call capability benchmark.

Provider keys are entered without terminal echo, masked in normal output, and kept outside the repository.

## Guarded OpenVMS editing

Ordinary `AGENT/WRITE` treats an existing-file edit as one atomic materialized write. The advertised existing-file primitive is `structured_patch`, which can carry multiple exact non-overlapping hunks for one file through one validation/confirmation/write operation. Supervised `AGENT/FIX` retains the older replace primitives. Raising `auto_writes` does not turn ordinary `AGENT/WRITE` into a sequence of independent existing-file patches.

For cross-file transactional changes, use the change-set/transaction machinery rather than assuming one-file structured-patch atomicity extends across files.

## Native source build profiles

The guarded `build_source` agent tool can compile/interpret one source file using `LANGUAGE/<LANGUAGE>.BUILD` profiles. It requires full approval, guarded writes, and DCL execution. Profiles cover compiled, compile-only, interpreted, and DCL-procedure execution modes; native status and captured command output are returned to the agent.

This tool supplements rather than replaces a project's normal `BUILD.COM` or equivalent build procedure.

## Git and GitHub on OpenVMS

OpenVMS Git is treated as RMS-sensitive. GitHub branch content is the repository source of truth, while RMS-visible files and native build/runtime behavior are acceptance truth. Do not rely on porcelain/ref output or `$STATUS` alone when `.git` control files or physical RMS files can verify the fact directly. See `DOC/OPENVMS_GIT_QUIRKS.MD`.

Authenticated raw Git uses `GITAUTH`. Operator-facing:

```text
$ GITAUTH --check
```

performs a non-mutating dry-run push preflight against the active profile's current origin to verify write authorization. `GIT_AUTH_SETUP.COM` uses an offline/config-only check during installation so setup itself does not depend on network availability.

`GITAUTH` uses short-lived repository-scoped authentication data and disables interactive credential prompting. Do not replace it with plaintext credential-helper storage.

## Compatibility

The current canonical acceptance environment is OpenVMS V9.2-3 x86-64. The project nevertheless preserves historical DEC C/VAX-facing compatibility constraints where they remain part of the product contract: externally visible C/linker names must remain 31 characters or fewer, implicit function declarations are not acceptable, and VAX-facing C remains compatible with the project's C89/DEC C discipline unless a subsystem specifically requires otherwise.

## Security

Do not commit provider keys, GitHub tokens, passwords, authorization headers, or private `SYS$LOGIN` profile files. Use the least-permissive approval tier that fits the task, keep guarded writes/DCL disabled when not needed, and preserve `BUILD.COM` as the authoritative product build. See `SECURITY.MD`.

## Project philosophy

OVMS Agent adapts modern coding-agent workflows to native OpenVMS rather than pretending OpenVMS is Unix. DCL, RMS versions, OpenVMS filespecs, native compiler/linker behavior, and the observed Git port all influence the design.

This project was developed with extensive assistance from OpenAI-generated code.
