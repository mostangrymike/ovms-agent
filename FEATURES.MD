# OVMS Agent Features

This file is a plain-language catalog of capabilities present in the current promoted product through **M299**.

## Project inspection and context

OVMS Agent can inspect an OpenVMS project, show bounded directory trees, read file ranges, search source text, build C symbol/dependency information, and assemble project/Git context for AI-assisted work.

COBOL files ending in `.COB`, `.COBOL`, or `.CBL` are recognized as source; `.CPY` files can remain supporting context. Git-aware status/diff logic is RMS-sensitive and avoids assuming native Git can safely hash every multi-version working file directly.

## Interactive settings

`SETTINGS` provides persistent user configuration for guarded writes, DCL execution, active provider/model, maximum output tokens, approval policy, network allow/deny lists, autonomous turns, and autonomous write limits.

Ordinary settings live in `SYS$LOGIN:OVMS_AGENT_SETTINGS.DAT`; provider/model state remains in `SYS$LOGIN:OVMS_AGENT_CONFIG.DAT`. Supported legacy `OVMS_AGENT_*` logical names retain higher precedence and are reported as overrides.

Noninteractive forms include `SETTINGS SHOW`, `GET`, `SET`, `TOGGLE`, `RESET`, and `RELOAD`.

## Native multiline prompt and goal input

M299 adds native `.END`-terminated multiline entry for commands whose remaining operand is a free-form LLM prompt or goal. Normal one-line forms remain immediate; multiline collection starts only when an eligible command has all required fixed leading arguments but no free-form tail.

For example:

```text
ASK
Explain this project.
Then summarize the main OpenVMS-specific constraints.
.END
```

Eligible no-fixed-argument forms include ASK, CHAT, AGENT, AGENT/WRITE, AGENT/BUILD, AGENT/FIX, AGENT/RETRY, AGENT/REPAIR, AGENT/CREATE, AGENT/PLAN, AGENT/EXEC, AGENT/EXEC/DRY, AGENT/EXEC/RESUME, AGENT/EXEC/RESUME/CREATE, and AGENT/AUTOPILOT.

Fixed-leading-argument forms enter multiline mode only after their structured prefix is complete:

```text
AGENT/IMAGE image-path
AGENT/SESSION/EXEC <id>
AGENT/EXEC/FORK <new-name> ::
```

M299 reuses the existing M251 multiline reader, so `.END` must appear by itself on a physical line; embedded newlines before it are preserved, and the existing overflow/EOF behavior remains authoritative. `AGENT/MULTILINE`, `AGENT/CREATE/MULTILINE`, and `AGENT/REPAIR/MULTILINE` remain compatibility aliases. Structured grammars such as DCL, RUN, PATCH, MCP, GitHub operations, and history/query commands are not broadened into implicit multiline input.

## AI provider profiles and health diagnostics

Named provider profiles store endpoint, model, and access-key state outside the repository. `PROVIDER LIST`, `SHOW`, `USE`, `ADD`, and `DELETE` manage them; `MODEL` shows or changes the active profile model.

M292 adds:

```text
PROVIDER TEST
PROVIDER TEST name
```

The test sends one bounded real no-op request through every configured profile or one named profile **without changing the active provider**. A responsive profile reports `OK`; failures are classified as `CONFIG`, `LOCAL ERROR`, `TRANSPORT`, `API ERROR`, `EMPTY`, or `INVALID`. ANSI-capable terminals use green/red presentation while textual states remain usable in plain output/logs.

The diagnostic uses real provider transport and a small amount of provider usage. It checks basic response health only; it is not a tool-call/capability benchmark. Dedicated temporary request/header/response files are removed across RMS versions.

Direct providers and OpenAI-compatible intermediaries such as Requesty are supported through configured endpoints. Multi-turn agent state is carried locally rather than requiring provider-held conversation identifiers.

### Tool transport compatibility selection

M294 preserves the common Responses API request contract while handling a proven Groq transport compatibility exception before send. When the configured endpoint is Groq, the outgoing request actually contains tool definitions, and the model is either `openai/gpt-oss-*` or the M295-proven `qwen/qwen3.6-27b`, OVMS Agent selects the existing bounded blocking transport instead of attempting SSE streaming. M296 adds the same pre-send selection for the OpenRouter endpoint when the configured model is exact `openrouter/free` and the serialized request contains tool definitions.

This is capability selection, not a failed-stream retry: the request is sent once through the compatible transport, avoiding duplicate requests after a real network failure. Non-tool requests and other provider/model combinations keep the existing streaming decision. Users do not need to define `OVMS_AGENT_NOSTREAM` for the supported Groq GPT-OSS/Qwen or OpenRouter/free tool workflows.

### Reasoning/output-budget exhaustion handling

M297 hardens plain ASK response handling when a provider returns an incomplete response because `max_output_tokens` was reached before any assistant text was produced. Instead of falling through to the raw provider-response fallback, OVMS Agent emits a bounded diagnostic explaining the exhaustion condition, includes reported output/reasoning token counts when available, and suggests retrying with a larger output budget or a more constrained prompt. Normal successful responses and unrelated provider failures keep their existing behavior.

## Guarded source changes

OVMS Agent can create and modify project files through guarded OpenVMS-aware paths.

After a successful automatic write, cached reads are invalidated so subsequent reads see the newest RMS version.

### Atomic ordinary AGENT/WRITE edits

Ordinary `AGENT/WRITE` existing-file work preserves a one-materialized-write invariant. The advertised existing-file primitive is `structured_patch`: one action can contain multiple exact, non-overlapping hunks for one file and validates/previews/confirms them together before a single RMS write.

The older `replace_text` / `replace_lines` primitives remain available to supervised `AGENT/FIX`; increasing `auto_writes` does not allow sequential existing-file patching in ordinary `AGENT/WRITE`.

For coordinated cross-file work, use `CHANGESET`/transaction behavior rather than assuming one-file structured-patch atomicity spans files.

## Build and repair support

The repository/project build procedure remains authoritative. OVMS Agent can run controlled builds, preserve bounded failed-build evidence, reason from compiler/linker/test diagnostics, generate guarded repair candidates, rebuild, and retain repair history/results.

Supervised repair can protect explicitly required behavior and reject a candidate that would remove required code-like goal terms unless removal/disablement was itself requested.

## Guarded native source builds

M289 adds the agent `build_source` tool for one source file. It loads `LANGUAGE/<LANGUAGE>.BUILD` and runs only when full approval, guarded writes, and DCL execution are enabled.

Shipped profiles cover:

- MACRO32;
- Fortran;
- COBOL;
- Pascal;
- C++;
- BASIC;
- Java (compile-only);
- Python (interpreted);
- Perl (interpreted); and
- DCL procedure execution.

Compiled profiles compile/link, compile-only profiles compile once, interpreted profiles invoke the configured interpreter, and DCL profiles execute one validated project-relative command procedure. Native status and captured output are returned to the agent.

## Language knowledge packs

The provider-neutral language layer can inject bounded native-validated guidance from `LANGUAGE/<LANGUAGE>.MD` when strong task/file evidence identifies a language. The currently shipped curated knowledge packs are COBOL and Fortran.

Knowledge packs are separate from `.BUILD` toolchain profiles: a language may have a build profile without a curated knowledge `.MD` pack.

## Platform knowledge packs

M298 adds a separate platform-knowledge namespace under `KNOWLEDGE/`, initially shipping `OPENVMS_GIT.MD`, `OPENVMS_RMS.MD`, and `OPENVMS_DCL.MD`. Strong prompt-topic evidence can inject one or more relevant packs into request instructions; unrelated prompts remain unchanged.

Each platform pack is bounded to 8 KiB and the combined platform-knowledge budget is 16 KiB. Relevant Git/RMS/DCL packs may compose with one another and with an M288 language pack. `OVMS_AGENT.COM` exposes the shipped directory through `OVMS_AGENT_KNOWLEDGE_DIR` while preserving an explicit preexisting override for custom installation or testing.

Platform packs are guidance only. They cannot override sandbox, safety, evidence, approval, write, network, or tool policy, and loading a pack is never evidence that any current ref, file, status, record format, logical name, or system state actually exists. Provider-free regression coverage verifies positive topic detection, combined-topic behavior, false-positive suppression, actual pack injection, unchanged unrelated requests, and composition with the existing language layer.

## Git context and RMS integrity

The Git/RMS engine treats GitHub/tree state, Git metadata, and physical RMS-visible files as separate evidence. It can compare committed content against current RMS versions without blindly asking Git to hash problematic multi-version files directly.

M290 reliability hardening fixed RMS structured-patch integrity, subprocess/native-status handling, Git/RMS short-read paths, persistent-settings test isolation, and bounded provider timeout behavior.

M291 filters OVMS Agent's own exact Git-status scratch filenames out of synthesized untracked status while preserving tracked same-name paths and lookalikes.

## GitHub profiles and authenticated Git

GitHub profiles live outside the repository in `SYS$LOGIN:OVMS_AGENT_GITHUB.DAT` and store repository/user/branch/token state. Tokens are entered without normal terminal echo and masked in normal output.

`GITAUTH` is the standing prompt-free raw Git network wrapper. It uses short-lived repository-scoped authentication and disables interactive credential prompting.

Operator-facing:

```text
GITAUTH --check
```

performs a non-mutating authenticated dry-run push to the active profile/current origin to verify **write authorization**, catching stale/wrong profile and under-authorized-token cases that a config-presence check cannot. Setup uses a config-only check.

Guarded FETCH/PULL/PUSH require repository/profile matching; PUSH and service mutations retain their approval requirements.

## OpenVMS Git/RMS operating safeguards

Because the observed OpenVMS Git port can return misleading/blank porcelain or update metadata without materializing RMS-visible files, the accepted workflow independently verifies `FETCH_HEAD`, `HEAD`, branch-ref files, index metadata, and physical files where relevant. `GIT_RESTORE.COM` provides the established RMS-aware tracked-file restore path.

See `DOC/OPENVMS_GIT_QUIRKS.MD` for exact operating rules.

## Safety controls

OVMS Agent combines approval policy, guarded-write and DCL gates, path/sensitive-file checks, network policy, autonomous budgets, transaction/rollback behavior, repository-bound authentication, and AUTOPILOT's inability to satisfy FULL approval.

`AGENT/AUTOPILOT` is bounded local workspace repair/build orchestration; it cannot silently authorize arbitrary DCL, GitHub/PUSH, MCP/external work, or other FULL-gated operations.

## Sessions, plans, image input, and external tools

The product also includes persistent sessions/transcripts/results, saved implementation plans and plan validation/execution, image input for read-only agent analysis, structured refactoring/symbol tools, MCP/external tool surfaces under approval policy, GitHub issue/PR service operations, and detailed repair-history reporting.

## Compatibility

The current canonical native acceptance host is **OpenVMS V9.2-3 x86-64**. Native OpenVMS semantics take priority over Unix assumptions.

Historical DEC C/VAX compatibility remains deliberate: new linker-visible names must stay within the 31-character external-identifier limit, implicit declarations are not acceptable, and VAX-facing C follows the project's C89/DEC C discipline unless an existing subsystem requires otherwise.
