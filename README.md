OVMS Agent

OVMS Agent is a native agentic programming assistant for OpenVMS.

It is designed to bring modern AI-assisted software development workflows to OpenVMS while respecting the platform's native conventions, build procedures, filesystem semantics, security model, and development tools.

The project is written primarily in C and is intended to run directly on OpenVMS without requiring a Unix compatibility layer for normal operation.

What OVMS Agent Can Do

OVMS Agent provides an interactive OpenVMS programming assistant with features such as:

project and source-tree inspection

plain-English source-code explanation

text and code lookup

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

external tool connections

guarded GitHub operations

persistent AI provider and GitHub profiles

interactive persistent user settings

multilingual OpenVMS development support

The current language-awareness layer explicitly supports C, OpenVMS commands, MACRO-32, Fortran, BASIC, Pascal, COBOL, and BLISS.

OVMS Agent does not assume that every compiler is installed. The project's own BUILD.COM remains authoritative for how a project is built.

Interactive Settings

Normal users can configure OVMS Agent from inside the program instead of defining OpenVMS logical names by hand.

Run:

SETTINGS

The numbered menu currently covers guarded writes, DCL execution, the active provider profile, the active model, the maximum output-token limit, approval policy, and network allow/deny lists. Entering the number of an ON/OFF setting toggles it immediately. Settings that need a value prompt for that value, and provider or approval choices use numbered submenus.

Ordinary settings are saved automatically in:

SYS$LOGIN:OVMS_AGENT_SETTINGS.DAT

Provider and model changes continue to use the existing provider-profile store in `SYS$LOGIN:OVMS_AGENT_CONFIG.DAT`, so provider credentials and model state are not duplicated.

Useful noninteractive forms are:

SETTINGS SHOW
SETTINGS GET name
SETTINGS SET name value
SETTINGS TOGGLE name
SETTINGS RESET
SETTINGS RELOAD

Existing `OVMS_AGENT_*` logical-name settings remain compatible for callers and automation that already use them. When a supported legacy logical is defined, it overrides the corresponding saved ordinary setting; SETTINGS marks that condition instead of silently replacing the explicit environment.

AI Provider Configuration

OVMS Agent can use named AI provider profiles without rebuilding the program. A profile stores the service address, model, and service access key for one artificial intelligence service.

Profiles are kept in:

SYS$LOGIN:OVMS_AGENT_CONFIG.DAT

The service access key is entered without terminal echo and is masked in normal command output. The file is kept outside the project repository and is written with restrictive permissions.

Useful commands are:

PROVIDER LIST
PROVIDER SHOW
PROVIDER USE name
PROVIDER ADD name
PROVIDER DELETE name
MODEL
MODEL model-name

The active profile is used by ASK, CHAT, AGENT, build assistance, file creation, and other AI-backed workflows.

The configured service address may point directly to an AI provider or to an intermediary service such as Requesty that routes requests to multiple AI systems. OVMS Agent builds the Responses service address from the configured base address and does not require a program rebuild when the service or model changes.

Multi-turn agent work keeps the conversation state locally. It carries the earlier model output and tool results forward in the next request instead of depending on the AI provider to remember a previous response identifier. This makes multi-step AGENT work less dependent on provider-specific server behavior.

Hardened OpenVMS Project Editing

COBOL files ending in .COB, .COBOL, or .CBL are recognized as source code. .CPY copy files are treated as supporting project files so they are available as context without being mistaken for primary source files.

After a successful automatic write, saved file-reading results are discarded. A later read therefore opens the newest OpenVMS file version rather than returning older saved contents.

For bounded automatic multi-file changes, OVMS Agent tracks the exact file versions that existed before the run. If the run stops incomplete because of an automatic limit or error, all affected file changes are undone back to the pre-run state.

When the top-level BUILD command fails, the failure status and captured build output are saved for repair planning. Temporary capture files are removed, and stale failed-build evidence is removed after a later successful build.

Supervised repair also protects explicitly required behavior. If a proposed repair would remove a required code-like goal term such as LIST, the candidate is rejected before the normal patch confirmation and write path. An explicit request to remove or disable that feature is still allowed.

Git context on OpenVMS is RMS-aware. When a tracked working-tree file has multiple RMS versions, OVMS Agent resolves the current source through normal versionless RMS access and compares that newest version with the committed Git content through temporary single-version files. This avoids VSI Git hash/read failures on such files while preserving every user source version.

OpenVMS Requirements

OVMS Agent is developed for OpenVMS and uses native OpenVMS facilities. The current development environment is OpenVMS on x86-64. The project is also written with older DEC C and VAX constraints in mind where practical.

You will need OpenVMS, a suitable C compiler, TCP/IP networking for network-backed features, and HTTPS support for AI and GitHub access.

Git is strongly recommended. OVMS Agent uses Git context for safe source work, checkpoints, repository awareness, and GitHub integration.

On the VSI Git environment used during development, reliable HTTPS Git operation requires:

$ SET PROCESS/PARSE_STYLE=EXTENDED
$ SET PROCESS/PRIVILEGE=SHARE

SHARE must already be an authorized privilege for the account.

Installing OVMS Agent

Clone or copy the repository to an OpenVMS directory, set that directory as the current default, then run:

$ @BUILD

A successful build should end with:

Building OVMS Agent Version 2...
All regression tests passed.
Build completed successfully.
Run with: $ @OVMS_AGENT

Start the program with:

$ @OVMS_AGENT

Exit with:

QUIT

First Commands to Try

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

GitHub Profiles and Guarded Authentication

OVMS Agent can keep named GitHub profiles in:

SYS$LOGIN:OVMS_AGENT_GITHUB.DAT

Each profile stores a repository name, GitHub user name, branch, and token. Tokens are entered without terminal echo and are masked in normal output. They are not stored in the project repository.

Useful commands are:

GITHUB LIST
GITHUB SHOW
GITHUB USE name
GITHUB ADD name
GITHUB DELETE name

Guarded GitHub commands include:

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

Before FETCH, PULL, PUSH, or CLONE uses saved credentials, OVMS Agent checks the required OpenVMS Git environment, creates short-lived repository-scoped authentication data, disables interactive credential prompting, and verifies that the saved credentials can read the configured repository. Temporary authentication, probe, and command files are removed across OpenVMS file versions afterward.

For FETCH, PULL, and PUSH, the current checkout must identify the same repository as the active saved profile. If the checkout and profile do not match, the operation is refused rather than sending credentials to or fetching from the wrong repository.

Approval controls remain authoritative. Network Git work requires the appropriate workspace approval, while PUSH remains protected by FULL approval.

GitHub PR and issue service operations use the shipped GITHUB_BRIDGE.COM procedure. A normal `$ @OVMS_AGENT` launch automatically defines the user-mode logical name `OVMS_AGENT_GITHUB_BRIDGE` to the copy of GITHUB_BRIDGE.COM in that checkout when no bridge logical is already defined. If the caller, login environment, or system has already defined `OVMS_AGENT_GITHUB_BRIDGE`, OVMS Agent preserves that explicit override instead of replacing it.

The service bridge reuses the active profile in SYS$LOGIN:OVMS_AGENT_GITHUB.DAT; it does not create a second credential store. Its authorization header, JSON body, and API-response files are short-lived SYS$LOGIN temporary files, protected against group/world access where they contain credentials, and deleted across OpenVMS file versions after each request. Service operations remain guarded by the existing FULL approval policy.

Supported service grammar is:

AGENT/GITHUB/PR help
AGENT/GITHUB/PR list
AGENT/GITHUB/PR create BASE HEAD ISSUE TITLE
AGENT/GITHUB/ISSUES help
AGENT/GITHUB/ISSUES list [open|closed|all]
AGENT/GITHUB/ISSUES create TITLE

PR and issue titles are currently one whitespace-delimited token. Use hyphens in place of spaces when needed. `PR create` creates a pull request from HEAD into BASE and uses `Closes #ISSUE` as its body. `ISSUES list` defaults to open issues when the state is omitted. `ISSUES create` creates an issue with the supplied title and a short OVMS Agent bridge marker in the body.

For fine-grained GitHub tokens, PR listing requires access to the repository's pull requests and PR creation requires `Pull requests: write`; issue listing requires access to repository issues and issue creation requires `Issues: write`. Ensure the token is also authorized for the target repository. The classic token configuration proven during OVMS Agent development used `repo` scope for a private repository. If GitHub returns `Resource not accessible` or another authorization error, verify the endpoint permissions on the active saved token before changing bridge payload code.

Multilingual OpenVMS Support

OVMS Agent is aware of major OpenVMS development languages and tries to preserve each language's native conventions.

For C development, the project observes historical DEC C/VAX linker constraints. Newly introduced external linker-visible identifiers must be 31 characters or fewer.

Security Notes

Do not commit service access keys, GitHub tokens, passwords, or other secrets.

Keep authentication material outside project repositories.

Review plans before approving write operations.

Use Git checkpoints before significant changes.

Do not grant FULL approval casually.

Preserve the project's BUILD.COM as the authoritative build procedure.

Project Philosophy

OVMS Agent aims for practical modern agentic programming behavior adapted to OpenVMS. Where Unix-oriented behavior does not map cleanly to OpenVMS, the project favors native OpenVMS semantics: OpenVMS commands, OpenVMS filespecs and RMS versions, native build procedures, privilege handling, OpenVMS Git behavior, and architecture-appropriate compiler and linker constraints.

The goal is to make AI-assisted development feel native on OpenVMS rather than bolted onto it.

License

This project was developed with extensive assistance from OpenAI-generated code.

No open-source license has currently been selected. See the repository's LICENSE file if one is added in the future.
