# OVMS Agent Architecture

OVMS Agent is a native OpenVMS agentic programming assistant. Its architecture is organized around one central constraint: modern AI-assisted development behavior must operate through OpenVMS-native files, DCL procedures, RMS semantics, compiler/linker constraints, and explicit approval boundaries.

This document describes the major subsystem boundaries and how data and authority move through them.

## High-level structure

At a high level, OVMS Agent consists of:

1. a DCL launch/build layer;
2. an interactive command layer;
3. a project/file/RMS layer;
4. a provider-neutral LLM/agent layer;
5. settings and profile persistence;
6. guarded write/transaction machinery;
7. build/repair/autonomous orchestration;
8. Git and GitHub integration;
9. persistent sessions, transcripts, results, history, and project context.

The code is intentionally modular, but the build remains native DCL rather than introducing an external build framework.

## Launch layer

The normal entry point is:

```text
@OVMS_AGENT
```

`OVMS_AGENT.COM`:

- resolves the project and `[.BUILD]` directory from its own procedure filespec;
- verifies that `[.BUILD]OVMS_AGENT.EXE` exists;
- exposes the shipped `GITHUB_BRIDGE.COM` to the launched image when no explicit bridge logical already exists;
- supports a one-shot invocation mode through DCL parameters;
- enables the scoped C RTL stream-pipe feature required by the live network transport;
- runs the image and returns its status.

The launcher deliberately scopes temporary logical-name behavior to the image activation rather than permanently changing the caller's process environment when that is not required.

## Build layer

`BUILD.COM` is authoritative for the product build and regression sequence.

It delegates large module families to:

- `BUILD_COMMAND_MODULES.COM`;
- `BUILD_LLM_MODULES.COM`;
- `COMMAND_MODULES.OPT`;
- `LLM_MODULES.OPT`.

Generated objects, test images, logs, option-file copies, and the final executable live under `[.BUILD]`.

The build structure reflects architectural boundaries: command-facing code and LLM/agent code are assembled as coherent module groups, while core project/edit/util modules are linked with them into the final image.

See `BUILD.md` for detailed build behavior.

## Interactive command layer

The command subsystem lives primarily in `SRC/COMMAND*.C` and related include files.

Responsibilities include:

- parsing user commands;
- dispatching core/project/settings/provider/GitHub commands;
- enforcing syntax and command-specific policy;
- connecting interactive commands to the project, LLM, build, refactoring, repair, and service subsystems;
- presenting user-readable results.

Some historically evolved commands use specialized dispatch paths rather than one flat registry, so user-facing command documentation must be validated against both the registered command surface and special dispatch logic.

`OVMS_AGENT.HLP` is the authoritative human-oriented command/settings reference and explains both what commands do and when they should be used.

## Project and RMS layer

The project layer centers on source-tree awareness and OpenVMS filesystem behavior.

Important responsibilities include:

- project-root resolution;
- bounded directory/tree/file inspection;
- source-language classification;
- safe path handling;
- Git-aware project state;
- RMS-aware reading and comparison;
- avoiding stale file-version reads after writes.

OpenVMS file versioning is not treated as a Unix filename implementation detail. Where Git and RMS semantics conflict, OVMS Agent uses temporary/single-version techniques and guarded materialization rather than assuming ordinary Unix checkout/hash behavior.

`SRC/PROJECT.C`, `SRC/RMS_WRITE*`, edit/transaction modules, and related path/context helpers are key parts of this boundary.

## Guarded write and transaction layer

File changes are not intended to flow directly from model text to arbitrary writes.

The guarded write architecture provides:

- project/workspace scope checks;
- approval checks;
- guarded-write enablement;
- exact-replacement/patch primitives;
- transactional multi-file work;
- rollback when bounded automatic work stops incomplete;
- protection of explicitly required behavior through goal-guard checks;
- RMS-aware creation/replacement behavior.

Structured refactoring commands reuse these controlled paths rather than inventing independent file writers.

This layer is a major safety boundary. New write-capable features should reuse it whenever possible.

## LLM/provider layer

The provider-neutral AI subsystem lives primarily under `SRC/LLM*`.

It separates generic agent behavior from provider profile configuration so that the program is not tied to one model vendor.

Responsibilities include:

- request construction;
- response parsing;
- tool schema and tool result handling;
- local multi-turn conversation state;
- network transport;
- provider/model selection;
- prompt construction;
- project/context assembly;
- image/request specializations where supported;
- parity and capability checks.

The architecture supports direct providers and OpenAI-compatible intermediaries such as Requesty through configured endpoints rather than compile-time provider selection.

Generic architecture naming uses `LLM` or `provider`; genuinely provider-specific compatibility logic remains provider-specific.

## Settings and profile persistence

OVMS Agent separates ordinary settings from credential-bearing/provider-specific profile stores.

### Ordinary settings

Stored in:

```text
SYS$LOGIN:OVMS_AGENT_SETTINGS.DAT
```

These include normal operational choices such as guarded-write/DCL gates, approval policy, network allow/deny values, output limits, and autonomous limits.

Supported legacy `OVMS_AGENT_*` logical names remain higher-precedence compatibility overrides for corresponding settings.

### AI provider profiles

Stored in:

```text
SYS$LOGIN:OVMS_AGENT_CONFIG.DAT
```

A provider profile contains endpoint/model/access-key state. Provider/model configuration can change without rebuilding the executable.

### GitHub profiles

Stored in:

```text
SYS$LOGIN:OVMS_AGENT_GITHUB.DAT
```

A GitHub profile contains repository/user/branch/token information used by guarded GitHub operations and the authenticated Git wrapper.

Keeping these stores under `SYS$LOGIN` keeps user configuration and secrets outside the source checkout.

## Agent planning and tool execution

The interactive AI-assisted workflows build bounded project context and send requests through the selected provider.

Tool-capable agent flows can perform operations such as:

- read/search project files;
- inspect Git state;
- reason about symbols/dependencies;
- propose/write controlled changes;
- build and inspect failures;
- operate on saved plans/sessions/results;
- use guarded GitHub or external service paths when policy permits.

The agent loop carries prior model output/tool results locally instead of depending on a provider-specific server-side conversation identifier. This improves provider portability and keeps conversation state under local control.

## Build, repair, and history workflows

OVMS Agent treats builds and repairs as stateful engineering operations rather than one-shot shell commands.

Build workflows can capture:

- final OpenVMS status;
- compiler/linker/test output;
- failed-build evidence used by repair planning.

Repair workflows can:

- reason from captured build failures;
- generate candidate edits;
- apply guarded transactional changes;
- reject candidates that violate explicit required behavior;
- rebuild;
- record repair history and artifacts for review.

Repair history and comparison commands exist so later decisions can inspect what was attempted rather than losing all context after one run.

## Autonomous execution and AUTOPILOT

Autonomous agent work has explicit bounded limits.

Persistent SETTINGS control normal autonomous model/tool turns and write-action ceilings, while test-only overrides can replace them inside regression tests.

`AGENT/AUTOPILOT` adds a stronger safety floor:

- it is limited to bounded local workspace repair/build behavior;
- it reuses the normal guarded/transactional primitives;
- it cannot satisfy FULL approval;
- it cannot silently authorize arbitrary DCL, PUSH, GitHub mutations, MCP/external actions, or other FULL-only operations;
- one invocation consumes one cumulative autonomous budget;
- it must stop in an explicit terminal state;
- it produces a human-review artifact.

AUTOPILOT is therefore an orchestrator over existing safety primitives, not a bypass around them.

## Symbol and structured-refactoring subsystem

The symbol subsystem maintains a C-oriented index used for definition lookup, callers, dependency analysis, function/struct display, and structured refactoring operations.

Refactoring commands such as rename/extract/move are intended to combine static project knowledge with guarded edits and verification rather than perform unbounded text replacement.

VAX/DEC C external-identifier limits remain part of the correctness model for refactoring operations that introduce or rename linker-visible symbols.

## Language-awareness layer

OVMS Agent recognizes multiple OpenVMS development languages and adjusts context/guidance accordingly.

The current language-awareness layer includes C, DCL/OpenVMS commands, MACRO-32, Fortran, BASIC, Pascal, COBOL, and BLISS.

Language awareness is not the same as requiring every compiler to be installed. The target project's own build procedure remains authoritative.

COBOL-specific guards and source/copy-file classification are examples of language-aware behavior implemented without replacing the project's native compiler/build system.

## Git integration

Git is used for:

- repository state and diff context;
- safe project reasoning;
- milestone/ref management;
- source-history awareness;
- GitHub integration.

OpenVMS RMS semantics can make native Git worktree/index operations unreliable. The established project workflow therefore distinguishes:

- GitHub branch state as repository source of truth;
- VAX materialization/build/runtime as acceptance authority;
- RMS-aware restore operations for changed tracked files;
- `core.filemode=false` to suppress meaningless OpenVMS mode drift.

Authenticated raw Git network operations use the installed `GITAUTH` wrapper and short-lived repository-scoped configuration rather than plaintext credential storage.

## GitHub integration

OVMS Agent has two related GitHub paths.

### Guarded Git operations

Commands cover status/remote/check/fetch/pull/push/clone behavior with repository/profile matching and approval requirements.

### GitHub service bridge

`GITHUB_BRIDGE.COM` implements issue and pull-request service operations using the active saved GitHub profile.

The launcher automatically exposes the shipped bridge when no explicit override exists.

The bridge uses protected short-lived header/body/response files and cleans them across RMS versions after operations.

GitHub mutations remain FULL-gated.

## Persistence and local state

OVMS Agent deliberately persists different classes of state separately:

- ordinary settings;
- AI provider profiles;
- GitHub profiles;
- agent sessions/transcripts/results;
- repair/build history and review artifacts;
- project context/instructions where applicable.

Credential-bearing configuration remains outside the repository. Generated build artifacts remain under `[.BUILD]`. Source-controlled policy and documentation remain in the repository.

This separation makes it possible to replace/update a checkout without treating generated objects or local credentials as source files.

## Security boundaries

The architecture relies on several independent boundaries:

- project-root/path checks;
- approval policy;
- guarded-write gate;
- DCL-execution gate;
- network policy;
- autonomous budgets;
- AUTOPILOT's inability to satisfy FULL;
- repository/profile matching for authenticated GitHub work;
- short-lived credential transport;
- transactional rollback.

See `SECURITY.md` for the security policy and reporting guidance.

## Documentation ownership

The root documentation is intentionally divided by contract:

- `README.md` — overview and navigation;
- `INSTALL.md` — prerequisites/setup/first run;
- `BUILD.md` — build/regression mechanics;
- `CONTRIBUTING.md` — engineering and milestone workflow;
- `SECURITY.md` — credential and authority boundaries;
- `ARCHITECTURE.md` — subsystem/data-flow model;
- `CHANGELOG.md` — user-visible milestone history;
- `OVMS_AGENT.HLP` — command and SETTINGS reference;
- `RULES.md` — durable project-development rules.

When architecture changes, update the document that owns the affected contract instead of allowing the README to become the only source of truth.