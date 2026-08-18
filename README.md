OVMS Agent

OVMS Agent is a native agentic programming assistant for OpenVMS.

It is designed to bring modern AI-assisted software development workflows to OpenVMS while respecting the platform's native conventions, build procedures, filesystem semantics, security model, and development tools.

The project is written primarily in C and is intended to run directly on OpenVMS without requiring a Unix compatibility layer for normal operation.

What OVMS Agent Can Do

OVMS Agent provides an interactive DCL-friendly programming assistant with features such as:

project and source-tree inspection

plain-English source-code explanation

text and symbol search

Git-aware project context

guarded file creation and modification

structured multi-file patching

controlled builds using the project's existing BUILD.COM

build-error diagnosis

supervised repair and retry workflows

persistent agent sessions and transcripts

project instruction context

source and build-result history

approval-controlled external tool execution

MCP server integration

guarded GitHub operations

multilingual OpenVMS development support

The current language-awareness layer explicitly supports:

C

DCL

MACRO-32

Fortran

BASIC

Pascal

COBOL

BLISS

OVMS Agent does not assume that every compiler is installed. The project's own BUILD.COM remains authoritative for how a project is built.

Hardened OpenVMS Project Editing

OVMS Agent includes several protections specifically for real OpenVMS projects and RMS file versions.

COBOL files ending in .COB, .COBOL, or .CBL are recognized as source code. .CPY copy files are treated as supporting project files so they are available as context without being mistaken for primary source files.

After a successful automatic write, saved file-reading results are discarded. A later read therefore opens the newest OpenVMS file version rather than returning older saved contents.

For bounded automatic multi-file changes, OVMS Agent tracks the exact file versions that existed before the run. If the run stops incomplete because of an automatic limit or error, all affected file changes are undone back to the pre-run state. The agent also avoids making an unnecessary final artificial-intelligence request while partial changes are waiting to be undone.

When the top-level BUILD command fails, the failure status and captured build output are saved for repair planning. Temporary capture files are removed, and stale failed-build evidence is removed after a later successful build.

Supervised repair also protects explicitly required behavior. If a proposed repair would remove a required code-like goal term such as LIST, the candidate is rejected before the normal patch confirmation and write path. An explicit request to remove or disable that feature is still allowed.

OpenVMS Requirements

OVMS Agent is developed for OpenVMS and uses native OpenVMS facilities.

The current development environment is OpenVMS on x86-64. The project has also been developed with portability to older OpenVMS environments in mind, including VAX-era DEC C constraints.

Required

You will need:

OpenVMS

A C compiler suitable for OpenVMS

VSI C on current OpenVMS systems is appropriate.

DEC C compatibility is important to the project.

TCP/IP networking

Required for OpenAI API access and GitHub/network operations.

HTTPS support

The system must be capable of making outbound HTTPS connections.

An OpenAI API key

Required for AI requests.

Strongly Recommended

Git

Git is not required for every basic OVMS Agent operation, but it is strongly recommended.

OVMS Agent uses Git context extensively for safe source-code work, checkpoints, repository awareness, and GitHub integration.

On VSI OpenVMS systems, initialize Git as appropriate for your installation, for example:

$ @SYS$STARTUP:GIT$STARTUP.COM

For the VSI Git environment used during development, the following process settings are required for reliable HTTPS Git operation:

$ SET PROCESS/PARSE_STYLE=EXTENDED
$ SET PROCESS/PRIVILEGE=SHARE

SHARE must be an authorized privilege for the account before it can be enabled.

OVMS Agent includes:

AGENT/GITHUB/CHECK

which verifies the relevant OpenVMS Git network prerequisites before network Git operations.

OpenAI API Configuration

OVMS Agent requires an OpenAI API key for AI-backed commands.

Do not place the API key in source files or commit it to Git.

Configure the API key using the mechanism appropriate for your local OVMS Agent installation and account environment.

Keep credentials outside the repository.

Installing OVMS Agent

The simplest installation method is to clone the repository or copy the source tree to an OpenVMS directory.

A typical installation directory might be:

SYS$LOGIN:[OVMS_AGENT]

or any other directory where the user has permission to create and build files.

1. Clone or Copy the Repository

If Git is installed:

$ SET PROCESS/PARSE_STYLE=EXTENDED
$ @SYS$STARTUP:GIT$STARTUP.COM
$ SET DEFAULT SYS$LOGIN:[OVMS_AGENT]
$ GIT "clone" "https://github.com/mostangrymike/ovms-agent.git" "."

If Git is not available, transfer the repository files to the OpenVMS system using your normal file-transfer method.

2. Set the Default Directory

$ SET DEFAULT SYS$LOGIN:[OVMS_AGENT]

Adjust the directory specification to match where you installed the project.

3. Build OVMS Agent

Run:

$ @BUILD

A successful build should end with output similar to:

Building OVMS Agent Version 2...
All regression tests passed.
Build completed successfully.
Run with: $ @OVMS_AGENT

The build procedure is the authoritative source for the modules and tests required by the current version.

4. Start OVMS Agent

Run:

$ @OVMS_AGENT

You should see:

OVMS Agent
Native agentic programming assistant for OpenVMS
Version 0.3.0

OVMS-AGENT>

Exit with:

QUIT

First Commands to Try

Display available commands:

HELP

Inspect the project:

ROOT
TREE
STATUS

Ask a question:

ASK Explain this project in plain English.

Run the read-only agent:

AGENT Explain the architecture of this project. Do not modify anything.

Inspect language support:

AGENT/LANG

Check practical capability coverage:

AGENT/PARITY/FINAL

Check GitHub/OpenVMS network readiness:

AGENT/GITHUB/CHECK

Typical Development Workflow

A safe workflow is:

inspect -> plan -> review -> dry run -> approve -> execute -> build -> test -> commit

For example:

OVMS-AGENT> AGENT Explain the code involved in the requested change.
OVMS-AGENT> AGENT/PLAN Make the requested change.
OVMS-AGENT> AGENT/PLAN/SHOW
OVMS-AGENT> AGENT/PLAN/VALIDATE
OVMS-AGENT> AGENT/EXECUTE/DRY_RUN

After reviewing and approving the plan, execute it with the appropriate approval controls, then build from DCL:

$ @BUILD

Git checkpoints are strongly recommended after verified changes.

GitHub Integration

OVMS Agent includes guarded GitHub support.

Available commands include:

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

Network Git operations use the installed OpenVMS Git command.

Credentials remain Git's responsibility and are not embedded into generated DCL.

For HTTPS authentication to GitHub, use a GitHub personal access token at the password prompt rather than storing credentials in the repository.

Multilingual OpenVMS Support

OVMS Agent is aware of major OpenVMS development languages and tries to preserve each language's native conventions.

Useful commands include:

AGENT/LANG
AGENT/LANG/INFO C
AGENT/LANG/INFO DCL
AGENT/LANG/INFO MACRO-32
AGENT/LANG/INFO FORTRAN
AGENT/LANG/DETECT filename
AGENT/LANG/POLICY

For C development, the project also observes historical DEC C/VAX linker identifier constraints, including the 31-character external identifier limit where applicable.

MCP and External Tools

OVMS Agent supports approval-controlled MCP and external tool integration.

External operations are subject to approval levels and should be enabled only when required.

Use:

AGENT/TOOLS/EXT

to inspect extended tool capabilities.

Security Notes

Do not commit API keys, GitHub tokens, passwords, or other secrets.

Keep authentication material outside the project repository.

Review plans before approving write operations.

Use Git checkpoints before significant changes.

Do not grant FULL approval casually.

Treat external MCP and GitHub operations as privileged actions.

Preserve the project's BUILD.COM as the authoritative build procedure.

Project Philosophy

OVMS Agent aims for practical Codex-style agentic programming behavior adapted to OpenVMS.

Where Unix-oriented behavior does not map cleanly to OpenVMS, the project favors native OpenVMS semantics rather than pretending the system is Unix.

That includes:

DCL rather than shell assumptions

OpenVMS filespecs and RMS semantics

native build procedures

OpenVMS privilege handling

OpenVMS Git behavior

architecture-appropriate compiler and linker constraints

The goal is to make AI-assisted development feel native on OpenVMS rather than bolted onto it.

License

This project was developed with extensive assistance from OpenAI-generated code.

No open-source license has currently been selected. See the repository'sLICENSE file if one is added in the future.
