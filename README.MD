# OVMS Agent

OVMS Agent is a native agentic programming assistant for OpenVMS.

It brings modern AI-assisted software-development workflows to OpenVMS while respecting native build procedures, filesystem and RMS semantics, security boundaries, DCL conventions, and platform-specific compiler/linker constraints. The project is written primarily in C and is intended to run directly on OpenVMS without requiring a Unix compatibility layer for normal operation.

## Release Status

- **Current public release:** `1.0.0`
- **License:** MIT
- **Primary build procedure:** `BUILD.COM`
- **Native user reference:** `OVMS_AGENT.HLP`
- **Canonical documentation-history directory:** `DOC`

## Documentation

| File | Purpose |
| --- | --- |
| [`INSTALL.MD`](INSTALL.MD) | Prerequisites, installation, first run, provider setup, and authenticated Git setup. |
| [`BUILD.MD`](BUILD.MD) | Authoritative build procedure, generated artifacts, diagnostics, regressions, and VAX acceptance. |
| [`CONTRIBUTING.MD`](CONTRIBUTING.MD) | GitHub-first development workflow, RMS-aware VAX acceptance, compatibility rules, and milestone hygiene. |
| [`SECURITY.MD`](SECURITY.MD) | Credentials, approval boundaries, DCL/write/network safety, GitHub authentication, and vulnerability reporting. |
| [`ARCHITECTURE.MD`](ARCHITECTURE.MD) | Subsystem boundaries, persistence, guarded writes, provider flow, repair/autonomous execution, and Git/GitHub integration. |
| [`CHANGELOG.MD`](CHANGELOG.MD) | User-visible milestone/release history and the current unreleased section. |
| [`OVMS_AGENT.HLP`](OVMS_AGENT.HLP) | Complete OpenVMS HELP source for commands and persistent SETTINGS, including when and why to use each feature. |
| [`RULES.MD`](RULES.MD) | Durable project-development rules and acceptance discipline. |
| [`LICENSE`](LICENSE) | MIT License terms for use, modification, and redistribution. |

Historical parity and reconciliation records are kept under [`DOC`](DOC/).

## What OVMS Agent Can Do

OVMS Agent provides an interactive OpenVMS programming assistant with support for:

- project and source-tree inspection;
- plain-English source-code explanation;
- text and code lookup;
- Git-aware project context;
- guarded file creation and modification;
- structured multi-file patching;
- controlled builds through the project's existing `BUILD.COM`;
- build-error diagnosis;
- supervised repair and retry workflows;
- persistent agent sessions and transcripts;
- project instruction context;
- source and build-result history;
- approval-controlled external-tool execution;
- guarded GitHub operations;
- persistent AI-provider and GitHub profiles;
- persistent interactive user settings; and
- multilingual OpenVMS development support.

The language-awareness layer explicitly supports C, OpenVMS commands, MACRO-32, Fortran, BASIC, Pascal, COBOL, and BLISS.

OVMS Agent does not assume that every compiler is installed. A project's own build procedure remains authoritative.

## Requirements

You need:

- OpenVMS;
- a suitable C compiler;
- TCP/IP networking for network-backed features;
- HTTPS support for AI-provider and GitHub access; and
- Git, strongly recommended for repository context, checkpoints, and GitHub integration.

For the VSI Git environment used during development, reliable HTTPS Git operation requires:

```text
$ SET PROCESS/PARSE_STYLE=EXTENDED
$ SET PROCESS/PRIVILEGE=SHARE
```

`SHARE` must already be an authorized privilege for the account.

See [`INSTALL.MD`](INSTALL.MD) for setup details and [`BUILD.MD`](BUILD.MD) for the authoritative build workflow.

## Build and Run

From the project root:

```text
$ @BUILD
```

A successful `1.0.0` build ends with:

```text
Building OVMS Agent Version 1.0.0...
All regression tests passed.
Build completed successfully.
Run with: $ @OVMS_AGENT
```

Start OVMS Agent with:

```text
$ @OVMS_AGENT
```

Exit the interactive program with:

```text
QUIT
```

## First Commands to Try

```text
HELP
SETTINGS
ROOT
TREE
STATUS
PROVIDER LIST
PROVIDER SHOW
MODEL
ASK Explain this project in plain English.
AGENT Explain the architecture of this project. Do not modify anything.
AGENT/LANG
AGENT/PARITY/FINAL
AGENT/GITHUB/CHECK
```

## Interactive Settings

Run:

```text
SETTINGS
```

The interactive settings menu covers guarded writes, DCL execution, the active provider profile, active model, maximum output-token limit, approval policy, network allow/deny lists, autonomous model/tool turns, and autonomous write actions.

Ordinary settings are saved automatically in:

```text
SYS$LOGIN:OVMS_AGENT_SETTINGS.DAT
```

Provider and model state continues to use:

```text
SYS$LOGIN:OVMS_AGENT_CONFIG.DAT
```

Useful noninteractive forms are:

```text
SETTINGS SHOW
SETTINGS GET name
SETTINGS SET name value
SETTINGS TOGGLE name
SETTINGS RESET
SETTINGS RELOAD
```

Existing `OVMS_AGENT_*` logical-name settings remain compatible. When a supported legacy logical is defined, it overrides the corresponding saved ordinary setting; `SETTINGS` reports that condition rather than silently replacing the explicit environment.

## AI Provider Configuration

OVMS Agent supports named AI-provider profiles without requiring a rebuild. A profile stores the service address, model, and service access key for one provider.

Profiles are kept in:

```text
SYS$LOGIN:OVMS_AGENT_CONFIG.DAT
```

The service access key is entered without terminal echo and is masked in normal command output. The configuration file is outside the project repository and is written with restrictive permissions.

Useful commands include:

```text
PROVIDER LIST
PROVIDER SHOW
PROVIDER USE name
PROVIDER ADD name
PROVIDER DELETE name
MODEL
MODEL model-name
```

The configured service address may point directly to an AI provider or to an intermediary such as Requesty. Multi-turn agent work carries prior model output and tool results forward locally instead of depending on provider-specific server-side conversation state.

## Guarded OpenVMS Editing

OVMS Agent's write path is designed around OpenVMS behavior rather than POSIX assumptions.

- COBOL files ending in `.COB`, `.COBOL`, or `.CBL` are recognized as source code.
- `.CPY` files are available as supporting context without being treated as primary source files.
- After an automatic write, saved file-read results are discarded so later reads see the newest OpenVMS file version.
- Bounded automatic multi-file changes track the exact pre-run file versions and roll back affected files if the run stops incomplete.
- Failed top-level builds preserve bounded diagnostics for repair planning; a later successful build clears stale failure evidence.
- Supervised repair protects explicitly required behavior through goal checks before the normal patch/write path.
- Git context is RMS-aware and avoids assuming that native Git can safely hash every multi-version OpenVMS working file directly.

## GitHub Profiles and Guarded Authentication

OVMS Agent can keep named GitHub profiles in:

```text
SYS$LOGIN:OVMS_AGENT_GITHUB.DAT
```

Each profile stores a repository name, GitHub user name, branch, and token. Tokens are entered without terminal echo, masked in normal output, and kept outside the project repository.

Profile commands include:

```text
GITHUB LIST
GITHUB SHOW
GITHUB USE name
GITHUB ADD name
GITHUB DELETE name
```

Guarded GitHub commands include:

```text
AGENT/GITHUB
AGENT/GITHUB/STATUS
AGENT/GITHUB/REMOTE
AGENT/GITHUB/CHECK
AGENT/GITHUB/FETCH
AGENT/GITHUB/PULL
AGENT/GITHUB/PUSH
AGENT/GITHUB/CLONE
AGENT/GITHUB/ISSUES
AGENT/GITHUB/PR
```

Before credentialed Git operations, OVMS Agent checks the required OpenVMS Git environment, creates short-lived repository-scoped authentication data, disables interactive credential prompting, and verifies that the saved credentials can read the configured repository. Temporary authentication and command files are removed across OpenVMS file versions afterward.

For `FETCH`, `PULL`, and `PUSH`, the current checkout must identify the same repository as the active saved profile. A mismatch is refused rather than risking credential use against the wrong repository. Network Git operations remain subject to approval policy, and `PUSH` remains protected by `FULL` approval.

GitHub PR and issue service operations use `GITHUB_BRIDGE.COM`. A normal `$ @OVMS_AGENT` launch defines `OVMS_AGENT_GITHUB_BRIDGE` to the bridge in the current checkout when no explicit override already exists.

Supported service grammar includes:

```text
AGENT/GITHUB/PR help
AGENT/GITHUB/PR list
AGENT/GITHUB/PR create BASE HEAD ISSUE TITLE
AGENT/GITHUB/ISSUES help
AGENT/GITHUB/ISSUES list [open|closed|all]
AGENT/GITHUB/ISSUES create TITLE
```

PR and issue titles are currently one whitespace-delimited token; use hyphens where spaces are needed. Fine-grained GitHub tokens must have the repository permissions required by the requested operation.

## OpenVMS Language and Compatibility Notes

OVMS Agent is aware of major OpenVMS development languages and tries to preserve each language's native conventions.

For C development, the project observes historical DEC C/VAX constraints. Newly introduced external linker-visible identifiers must be 31 characters or fewer, and VAX-facing code remains compatible with the project's DEC C/C89 discipline unless an existing subsystem specifically requires otherwise.

## Security

- Do not commit service access keys, GitHub tokens, passwords, or other secrets.
- Keep authentication material outside project repositories.
- Review plans before approving write operations.
- Use Git checkpoints before significant changes.
- Do not grant `FULL` approval casually.
- Preserve the project's `BUILD.COM` as the authoritative build procedure.

See [`SECURITY.MD`](SECURITY.MD) for the full security model and reporting guidance.

## Project Philosophy

OVMS Agent aims for practical modern agentic programming behavior adapted to OpenVMS. When Unix-oriented behavior does not map cleanly to OpenVMS, the project favors native OpenVMS semantics: DCL commands, OpenVMS filespecs and RMS versions, native build procedures, privilege handling, OpenVMS Git behavior, and architecture-appropriate compiler/linker constraints.

The goal is to make AI-assisted development feel native on OpenVMS rather than bolted onto it.

## License

OVMS Agent is distributed under the MIT License. See [`LICENSE`](LICENSE) for the complete terms.

This project was developed with extensive assistance from OpenAI-generated code.
