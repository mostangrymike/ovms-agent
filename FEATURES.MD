# OVMS Agent Features

This file is a plain-language catalog of the capabilities currently present in OVMS Agent.

## Project inspection

OVMS Agent can inspect an OpenVMS project, show its directory tree, read files, search source text, and build code lookup information for artificial-intelligence-assisted work.

The project map recognizes C and COBOL source files. COBOL files ending in `.COB`, `.COBOL`, or `.CBL` are treated as source code. `.CPY` copy files are treated as supporting files so they remain available as context without being confused with primary source files.

## Interactive settings

`SETTINGS` provides a numbered interactive configuration menu so normal users do not need to define OpenVMS logical names to change ordinary OVMS Agent behavior.

The current menu covers guarded writes, DCL execution, the active provider profile, the active model, the maximum output-token limit, approval policy, network allow/deny lists, and the autonomous model/tool-turn and write-action limits. Entering the number of an ON/OFF setting toggles it immediately. Settings that need a value prompt for that value, and provider or approval choices use numbered submenus.

Ordinary settings are saved automatically in `SYS$LOGIN:OVMS_AGENT_SETTINGS.DAT` and are loaded on later launches. Provider and model changes continue to use the existing provider-profile store in `SYS$LOGIN:OVMS_AGENT_CONFIG.DAT` so provider credentials and model state are not duplicated.

`SETTINGS SHOW`, `SETTINGS GET name`, `SETTINGS SET name value`, `SETTINGS TOGGLE name`, `SETTINGS RESET`, and `SETTINGS RELOAD` provide noninteractive equivalents where useful. The autonomous limits use the names `auto_turns` and `auto_writes`; saved changes apply to the next autonomous agent run without restarting OVMS Agent.

Existing `OVMS_AGENT_*` logical-name configuration remains compatible. When a supported legacy logical is defined, it overrides the corresponding saved ordinary setting and the SETTINGS display marks that condition instead of silently replacing the caller's explicit environment.

## AI provider profiles

OVMS Agent can switch artificial intelligence services without rebuilding the program.

Named profiles are stored outside the project repository in `SYS$LOGIN:OVMS_AGENT_CONFIG.DAT`. Each profile keeps a service address, model, and service access key. The key is entered without terminal echo and is masked in normal output.

`PROVIDER LIST`, `PROVIDER SHOW`, `PROVIDER USE`, `PROVIDER ADD`, and `PROVIDER DELETE` manage profiles. `MODEL` shows or changes the model associated with the active profile.

A service address may point directly to an AI provider or to an intermediary such as Requesty. AI-backed commands use the active profile consistently.

Multi-turn agent work keeps its continuation information locally by carrying earlier model output and tool results into later requests. It does not require the artificial intelligence service to retain a previous response identifier.

## Guarded source changes

OVMS Agent can make bounded source changes while preserving native OpenVMS file behavior.

After a successful automatic write, saved file-reading results are discarded so the next read opens the newest OpenVMS file version rather than returning older saved contents.

For automatic multi-file work, OVMS Agent tracks the exact file versions that existed before the run. If the run stops incomplete because of an automatic limit or error, it restores all affected files to their pre-run state.

## Build and repair support

The project's `BUILD.COM` remains authoritative.

When the top-level `BUILD` command fails, OVMS Agent keeps the build status and captured output so repair planning can inspect the same failure the user just saw. Temporary capture files are removed, and stale failed-build evidence is removed after a later successful build.

Supervised repair protects explicitly required code behavior. If a repair candidate would remove a required code-like goal term such as `LIST`, OVMS Agent rejects that candidate before the normal patch confirmation and write path. An explicit request to remove or disable that feature is still allowed.

## Git context and RMS versions

Git-aware context resolves working-tree files through normal OpenVMS RMS versionless access. If a tracked file has multiple RMS versions, OVMS Agent compares the committed Git content with the newest RMS version by using temporary single-version files instead of asking Git to hash the RMS source file directly.

This keeps Git status, diff, changed-path, and model context usable when VSI Git would otherwise report short-read or hash failures, and it does not purge or delete any user source versions.

## GitHub profiles and guarded authentication

OVMS Agent can store named GitHub profiles outside the repository in `SYS$LOGIN:OVMS_AGENT_GITHUB.DAT`. A profile contains the repository, user name, branch, and token. Tokens are entered without terminal echo and are masked in normal output.

`GITHUB LIST`, `GITHUB SHOW`, `GITHUB USE`, `GITHUB ADD`, and `GITHUB DELETE` manage those profiles.

For guarded FETCH, PULL, PUSH, and CLONE operations, OVMS Agent checks the OpenVMS Git prerequisites, creates short-lived authentication data scoped to the configured repository, disables interactive credential prompting, and verifies that the saved credentials can read that repository before running the requested operation.

For FETCH, PULL, and PUSH, the current checkout must match the repository named by the active saved profile. A mismatch is refused instead of allowing credentials to be applied to the wrong repository.

Temporary authentication, probe, and command files are removed across OpenVMS file versions after the operation. Existing approval controls remain in force, including FULL approval for PUSH.

## Safety controls

OVMS Agent uses bounded write counts, approval checks, path safety checks, sensitive-file checks, OpenVMS file-version handling, controlled builds, rollback behavior, and repository-bound authentication checks to reduce the chance that an incomplete or misdirected operation damages a project.

Write and command-execution capabilities remain controlled rather than being enabled without user intent.

## OpenVMS compatibility

OVMS Agent favors native OpenVMS behavior rather than Unix assumptions. This includes OpenVMS commands, RMS file versions, `BUILD.COM`, OpenVMS Git behavior, and compatibility with older DEC C constraints where practical.

New linker-visible names must remain within the 31-character external identifier limit required by VAX-era DEC C environments.

## Additional capabilities

OVMS Agent also includes read-only project assistance, implementation planning, structured file creation and modification, Git-aware context, persistent sessions and transcripts, language awareness for major OpenVMS development languages, image input, and approval-controlled external tool connections.

The exact command set continues to evolve, but safety and native OpenVMS behavior take priority over imitating Unix-oriented development tools.
