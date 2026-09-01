# OVMS Agent Architecture

OVMS Agent is a native OpenVMS agentic programming assistant. Its architecture is organized around one central constraint: modern AI-assisted development must operate through OpenVMS-native files, DCL procedures, RMS semantics, compiler/linker behavior, provider-neutral transport, and explicit approval boundaries.

The current canonical native acceptance host is **OpenVMS V9.2-3 x86-64**. Historical DEC C/VAX compatibility remains an explicit source/linker constraint where applicable.

## High-level structure

OVMS Agent consists of:

1. a DCL launch/build layer;
2. an interactive command layer;
3. a project/file/RMS layer;
4. a provider-neutral LLM/agent layer;
5. settings/profile persistence;
6. guarded write and transactional machinery;
7. build/repair/autonomous orchestration;
8. language knowledge and native source-build profiles;
9. Git/GitHub integration; and
10. persistent sessions, transcripts, results, history, and project context.

## Launch and product build

The normal entry point is:

```text
@OVMS_AGENT
```

`OVMS_AGENT.COM` resolves the checkout/build image, exposes the shipped GitHub bridge and language directory unless explicitly overridden, enables the scoped stream-pipe behavior required by network transport, and runs the image.

`BUILD.COM` is authoritative for the OVMS Agent product build and canonical regressions. Command and LLM module groups are built through their dedicated DCL procedures/options files; generated objects, tests, logs, and `OVMS_AGENT.EXE` live under `[.BUILD]`.

## Interactive command layer

`SRC/COMMAND*.C` and related modules parse and dispatch project, SETTINGS, provider, GitHub, agent, repair, session, refactoring, and tool commands while enforcing command-specific syntax/policy.

`OVMS_AGENT.HLP` is the authoritative human-oriented command/SETTINGS reference. User-visible command/settings changes must update it in the same milestone.

## Project and RMS layer

The project/RMS layer handles:

- project-root resolution and path safety;
- bounded directory/tree/file inspection;
- source classification and project context;
- RMS-aware reading/comparison;
- Git-aware project state; and
- stale-version avoidance after writes.

OpenVMS file versions are not treated as a Unix implementation detail. Where the observed Git port and RMS semantics conflict, OVMS Agent uses verified Git metadata plus RMS-aware materialization/comparison rather than assuming ordinary checkout/hash behavior is sufficient.

## Guarded writes and transactions

Model text is not allowed to become arbitrary filesystem writes directly. The guarded architecture combines project/workspace scope, approval policy, the guarded-write gate, RMS-aware write primitives, confirmation, exact change accounting, and rollback/transaction mechanisms.

### Ordinary AGENT/WRITE existing-file behavior

M291 preserves a **one materialized existing-file write** invariant for ordinary `AGENT/WRITE`. Its advertised existing-file primitive is `structured_patch`, which may contain multiple exact non-overlapping hunks for **one file** and validates/previews/confirms them together before one RMS write.

`replace_text` and `replace_lines` remain available to supervised `AGENT/FIX`, but are not ordinary `AGENT/WRITE` existing-file primitives. Raising `auto_writes` does not turn ordinary `AGENT/WRITE` into sequential existing-file patching.

This is one-file atomicity only. Cross-file all-or-rollback work belongs to `CHANGESET`/transaction machinery.

Model-invoked `structured_patch` uses a local confirmation path before mutation. Language-specific structural guards remain separate concerns; do not infer that every structured patch automatically invokes every language guard.

## LLM/provider layer

The provider-neutral subsystem under `SRC/LLM*` separates generic agent behavior from profile-specific endpoint/model/access-key configuration.

Responsibilities include:

- request construction and response parsing;
- provider/model selection;
- network transport and bounded timeout behavior;
- tool schemas/results and multi-turn local continuation;
- prompts/project/language context;
- image requests where supported; and
- provider compatibility handling when protocol behavior genuinely differs.

Direct providers and OpenAI-compatible intermediaries such as Requesty are selected through configuration rather than compile-time provider choice. Generic architecture naming uses `LLM`/`provider`; OpenAI naming is reserved for genuinely OpenAI-specific behavior.

### Provider health diagnostics

`PROVIDER TEST` / `PROVIDER TEST name` use the real provider transport with dedicated temporary request/header/response files while leaving the selected active profile unchanged. A profile is `OK` only when configuration is usable, transport/auth/request acceptance succeeds, the response parses, and nonempty assistant text is obtained.

Other states distinguish `CONFIG`, `LOCAL ERROR`, `TRANSPORT`, `API ERROR`, `EMPTY`, and `INVALID`. This diagnostic tests basic response health, not tool-call/capability compatibility.

### Tool transport compatibility selection

M294-M296 keep the Responses API as the common request contract while separating protocol choice from transport mode. Before a request is sent, the transport layer inspects the configured endpoint/model plus whether the serialized request actually contains tool definitions.

For the proven Groq `openai/gpt-oss-*` tool-call compatibility case and the M295-proven Groq `qwen/qwen3.6-27b` tool workflow, the request is sent once through the existing bounded blocking Responses transport instead of SSE streaming. M296 adds the same narrow pre-send decision for the OpenRouter endpoint when the configured model is exact `openrouter/free` and the request actually contains tool definitions. This is a pre-send capability decision, not a retry after a failed stream, so it does not duplicate a request after an actual network/child-process failure. Non-tool traffic and other provider/model combinations continue through the existing streaming decision unless the user has independently disabled streaming.

The selector is deliberately narrow and provider/model evidence-driven. It does not make `PROVIDER TEST` a tool capability test and does not introduce a second API dialect.

## Settings and profile persistence

Ordinary settings:

```text
SYS$LOGIN:OVMS_AGENT_SETTINGS.DAT
```

Provider profiles:

```text
SYS$LOGIN:OVMS_AGENT_CONFIG.DAT
```

GitHub profiles:

```text
SYS$LOGIN:OVMS_AGENT_GITHUB.DAT
```

Credential-bearing state stays outside the source checkout. Supported legacy logical names remain higher-precedence compatibility overrides for corresponding ordinary settings.

## Agent planning, tools, and native source builds

Agent workflows assemble bounded project/context state and use the selected provider. Tool-capable flows can inspect files/Git/symbols, make guarded changes, build, operate on sessions/plans/results, and use guarded external/GitHub paths when policy allows.

M289 added guarded `build_source` execution for one native source file using `LANGUAGE/<LANGUAGE>.BUILD` profiles. It requires full approval, guarded writes, and DCL execution. Profiles can represent compiled/link, compile-only, interpreted, and project-relative DCL procedure execution. Shipped profiles cover MACRO32, Fortran, COBOL, Pascal, C++, BASIC, Java, Python, Perl, and DCL.

`build_source` supplements rather than replaces a project's authoritative build procedure.

## Language knowledge layer

Shared language knowledge is data-driven. The launcher exposes `LANGUAGE/` through `OVMS_AGENT_LANGUAGE_DIR`; strong task/file evidence can load a bounded `<LANGUAGE>.MD` pack into request instructions. The guidance is explicitly subordinate to safety/evidence/approval/write policy.

The currently shipped curated knowledge packs are:

- `LANGUAGE/COBOL.MD`;
- `LANGUAGE/FORTRAN.MD`.

Knowledge packs and `.BUILD` toolchain profiles are separate mechanisms: a language may have a build profile without a curated `.MD` knowledge pack.

## Builds, repairs, and autonomous execution

Build workflows capture native status and diagnostics. Repair workflows reason from captured failures, generate guarded candidates, protect required behavior, rebuild, and record history/artifacts.

Persistent `auto_turns`/`auto_writes` settings bound autonomous work. `AGENT/AUTOPILOT` adds a hard safety floor: it automates bounded local workspace repair/build behavior, cannot satisfy `FULL`, cannot silently authorize GitHub/PUSH/MCP/arbitrary-DCL work, consumes a cumulative budget, and stops in an explicit terminal state with human-review evidence.

## Symbol/refactoring subsystem

The C-oriented symbol subsystem supports definitions, callers, module/dependency analysis, and structured refactoring such as rename/extract/move. Refactoring must preserve guarded-write and verification boundaries.

New externally visible identifiers remain subject to the historical DEC C/VAX 31-character compatibility limit even though the current canonical acceptance host is x86-64.

## Git integration

Git is used for repository context, milestone/ref management, history, and GitHub integration. The established architecture distinguishes:

- GitHub branch/tree state as repository truth;
- RMS-visible OpenVMS files as native compile/runtime input truth;
- Git index/ref metadata as a separate state that may require independent verification; and
- exact-SHA native acceptance from tree-equivalent promotion.

The observed OpenVMS Git port can return misleading/blank porcelain or update metadata without reconstructing physical RMS files. `DOC/OPENVMS_GIT_QUIRKS.MD` documents the accepted `FETCH_HEAD`, `read-tree`, `update-ref`, `symbolic-ref`, and `GIT_RESTORE` workflow.

M290/M291 hardening also keeps internal Git-status scratch files out of synthesized user status and preserves native/POSIX status semantics rather than trusting simple odd/even checks.

## GitHub authentication and service integration

Authenticated raw Git uses `GITAUTH` with short-lived repository-scoped configuration and interactive prompting disabled. `GITAUTH --check` performs a non-mutating authenticated dry-run push preflight to verify current-origin write authorization; setup uses a config-only check.

Guarded GitHub commands and `GITHUB_BRIDGE.COM` provide repository and issue/PR service operations under approval rules. GitHub mutations remain FULL-gated where required. Temporary authorization/body/response data is protected and removed across RMS versions.

## Persistence and security boundaries

OVMS Agent separates ordinary settings, provider/GitHub profiles, sessions/transcripts/results, repair/build history, project context, and generated build artifacts by responsibility. Credential-bearing state stays outside the repository.

Independent safety boundaries include:

- project-root/path checks;
- approval policy;
- guarded-write gate;
- DCL-execution gate;
- network policy;
- autonomous budgets;
- AUTOPILOT's inability to satisfy FULL;
- repository/profile matching;
- short-lived credential transport; and
- transaction/rollback behavior.

See `SECURITY.MD` for the security contract.

## Documentation ownership

- `README.MD` — overview/navigation;
- `INSTALL.MD` — setup/first run;
- `BUILD.MD` — product build/acceptance;
- `CONTRIBUTING.MD` — engineering workflow;
- `SECURITY.MD` — credential/authority boundaries;
- `ARCHITECTURE.MD` — subsystem/data-flow model;
- `FEATURES.MD` — current capability catalog;
- `CHANGELOG.MD` — promoted user-visible history;
- `OVMS_AGENT.HLP` — command/SETTINGS reference;
- `RULES.MD` — durable current project rules;
- `DOC/` — historical reconciliation/parity records and the current Git/RMS operating guide.
