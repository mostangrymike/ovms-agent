# OVMS Agent Features

This file is a plain-language catalog of the capabilities currently present in OVMS Agent.

## Project inspection

OVMS Agent can inspect an OpenVMS project, show its directory tree, read files, search source text, and build a code map for artificial-intelligence-assisted work.

The project map recognizes C and COBOL source files. COBOL files ending in `.COB`, `.COBOL`, or `.CBL` are treated as source code. `.CPY` copy files are treated as supporting files so they remain available as context without being confused with primary source files.

## Guarded source changes

OVMS Agent can make bounded source changes while preserving native OpenVMS file behavior.

After a successful automatic write, saved file-reading results are discarded so the next read opens the newest OpenVMS file version rather than returning older saved contents.

For automatic multi-file work, OVMS Agent tracks the exact file versions that existed before the run. If the run stops incomplete because of an automatic limit or error, it restores all affected files to their pre-run state. It also avoids making an unnecessary final artificial-intelligence request while partial changes are still waiting to be undone.

## Build and repair support

The project's `BUILD.COM` remains authoritative.

When the top-level `BUILD` command fails, OVMS Agent keeps the build status and captured output in its failed-build record so repair planning can inspect the same failure the user just saw. Temporary capture files are removed, and stale failed-build evidence is removed after a later successful build.

Supervised repair protects explicitly required code behavior. If a repair candidate would remove a required code-like goal term such as `LIST`, OVMS Agent rejects that candidate before the normal patch confirmation and write path. An explicit user request to remove, delete, disable, drop, or omit that feature is still allowed.

## Safety controls

OVMS Agent uses bounded write counts, approval checks, path safety checks, sensitive-file checks, OpenVMS file-version handling, controlled builds, and rollback behavior to reduce the chance that an incomplete agent run leaves a project in a partially changed state.

Write and command-execution capabilities remain controlled rather than being enabled without user intent.

## OpenVMS compatibility

OVMS Agent favors native OpenVMS behavior rather than Unix assumptions. This includes OpenVMS commands, RMS file versions, `BUILD.COM`, OpenVMS Git behavior, and compatibility with older DEC C constraints where practical.

New linker-visible names must remain within the 31-character external identifier limit required by VAX-era DEC C environments.

## Additional capabilities

OVMS Agent also includes read-only project assistance, implementation planning, structured file creation and modification, Git-aware context, guarded GitHub operations, persistent sessions and transcripts, language awareness for major OpenVMS development languages, image input, and approval-controlled external tool connections.

The exact command set continues to evolve, but safety and native OpenVMS behavior take priority over imitating Unix-oriented development tools.