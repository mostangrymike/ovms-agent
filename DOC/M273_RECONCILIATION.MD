# M273 ANSI Presentation Reconciliation

## Scope

M273 introduces a self-contained ANSI terminal-presentation layer for OVMS Agent. Presentation is deliberately separated from write, approval, parsing, networking, and provider control flow so the feature can be bypassed or removed without changing project semantics.

## ANSI instead of SMG$

The project does not need a full-screen terminal application. The visible goals are bounded: colorized diff lines, incremental assistant text, and one carriage-return/repaint status line. Standard SGR and erase-line sequences are sufficient for VT-series terminals and DECterm-style environments without adding an SMG$ screen-management dependency or a multi-pane TUI architecture.

ANSI emission is therefore centralized in `SRC/ANSI_TERM.C`. Callers request presentation operations from this module rather than embedding escape sequences themselves.

## Plain-output safety rule

ANSI output is opt-in by positive capability detection, not opt-out by assumption. `ANSI_TERM` asks OpenVMS for the `DVI$_DEVCLASS` of `SYS$OUTPUT` and enables ANSI only when OpenVMS identifies it as `DC$_TERM`. Redirected files, pipes/mailboxes, batch output, and other non-terminal devices remain plain.

An earlier M273 candidate additionally gated terminal output on `DEV$M_ODV` and `DEV$M_DET`. VAX probing showed that this was too restrictive for the live interactive environment, so those extra characteristic gates were removed.

The accepted Phase 0/1 VAX gate reported `SYS$OUTPUT DEVCLASS=66 DEVNAM=_FTA5:` and `ANSI terminal probe: enabled=1 forced_plain=0`. The same module passed redirected-output and `OVMS_AGENT_PLAIN` captures with a byte-level verifier that rejects byte `0x1B`; both captures contained no escape byte.

`OVMS_AGENT_PLAIN` and `OVMS_AGENT_NOCOLOR` are explicit presentation escape hatches. If either is defined, plain mode wins before terminal detection is consulted. Persistent status-line operations are a no-op in plain mode so existing batch and machine-driven output does not gain new lines.

## Presentation routing and accepted visual gate

The production integration keeps presentation decisions out of mutation/control-flow code:

- assistant response/error/raw-response display routes through `ANSI_TERM`;
- `GITDIFF` and repair final-diff rendering use an ANSI diff callback registered by `MAIN`, while legacy minimal-link tests retain their original plain fallback when no callback is registered;
- AGENT/WRITE exact-text and line-range Current/Proposed previews route stdout presentation through `ANSI_TERM` while file streams and transaction logic remain ordinary stdio;
- AGENT/PATCH adds an interactive-only colored structured-patch preview after the existing validator succeeds; plain mode adds no preview bytes;
- saved-plan/approval stdout routes through a presentation wrapper without changing approval text or yes/no semantics.

The first integrated VAX interactive check found two OpenVMS Git/DCL compatibility issues. A Git invocation using `-c color.diff...` and `--color=always` returned success but emitted no diff, so M273 no longer depends on Git to emit ANSI. The replacement initially ended a DCL command in Git's `--` end-of-options token; DCL interpreted the trailing hyphen as a continuation marker. The accepted implementation therefore runs `git diff --no-color --output=OVMS_AGENT_ANSI_DIFF.TMP`, classifies added/deleted/context lines inside `ANSI_TERM`, emits SGR locally, and removes all temporary OpenVMS file versions. Plain mode deliberately keeps the pre-M273 `git diff --` command unchanged.

The live visual gate at `8fea9feb97abc955dccdfacb5e5675fd63021b48` was accepted: real GITDIFF additions were green with dim context/header lines; AGENT/PATCH OLD text was red, NEW text green, markers/context dim; write-disabled semantics still refused the patch; and the gray `turn 1/2` status occupied one reusable line, disappeared before the response, and left the next prompt intact. The fresh full VAX build at that checkpoint also passed all regressions.

## Status and metrics boundary

The interactive agent loop shows the current turn against the active `AGENT/AUTO/LIMITS` ceiling while a provider request is in flight. The first streamed text delta clears that status line once before incremental text is written; later deltas do not erase earlier streamed text. Tool-only responses clear the status after the provider request completes.

The pre-M273 `AGENT/METRICS` implementation does not collect provider input tokens, output tokens, total tokens, or monetary cost. M273 therefore does not synthesize values. The completed Responses object can carry real provider `usage` fields; those can be reconciled into metrics only after the streaming transport is VAX-accepted. Monetary cost additionally requires trustworthy provider/model pricing data or provider-supplied cost information.

## Streaming is a networking capability

The original provider transport invokes `curl` with `--output LLM_RESPONSE_FILE` through `system()`, so the parent waits for a complete response before parsing it. Merely printing that completed response in chunks would not be streaming and is intentionally rejected.

M273 adds a separate incremental path while preserving that original blocking path as the default/fallback contract:

1. Streaming is considered only when `ANSI_TERM` has positively identified a real interactive terminal. Non-terminal/plain sessions continue through the original blocking request path.
2. `OVMS_AGENT_NOSTREAM` independently disables streaming without disabling ANSI colors or the status line.
3. A temporary copy of the already-built JSON request receives `"stream":true`; the original request file is left unchanged.
4. The transport starts `curl --no-buffer` with DEC C `popen()` and reads the child stream incrementally.
5. `SRC/LLM_SSE.C` parses SSE records. `response.output_text.delta` text is JSON-decoded and passed to `ansi_term_stream()` immediately.
6. `response.completed`, `response.failed`, or `response.incomplete` supplies a nested final `response` object. The parser writes that object back as the ordinary `LLM_RESPONSE_FILE`, preserving all existing response-id, tool-call, context, and final-response parsers.
7. If a provider returns ordinary JSON instead of SSE, the parser stores that JSON as the completed response without inventing incremental text.
8. If the streaming subprocess cannot be started, the request falls back to the original blocking path. After an actual network stream has started, parse/protocol failure does not silently resend the request.

AGENT/PLAN intentionally remains completed-response presentation in M273 because its final response text is also persisted as the plan artifact. Other ordinary response/final-answer paths suppress their completed-text print when deltas were already displayed, preventing duplicate assistant output.

A focused M273 regression feeds synthetic `response.output_text.delta` and `response.completed` SSE records through the parser, verifies the exact streamed text, then verifies that the reconstructed response still parses through the existing response-id/output-text functions. The same regression exercises DEC C `popen()`/`pclose()` as a live parent/child pipe. This regression is part of canonical `@BUILD`; OpenVMS/VAX remains authoritative for the C RTL pipe behavior.

## M273 sequencing

1. VAX-validate the isolated ANSI module and plain-output invariants. **Accepted.**
2. Wire the module and focused no-ESC regressions into the canonical build. **Accepted.**
3. Route diff/approval, assistant-response, and turn-status presentation without changing write/approval semantics. **Accepted.**
4. Validate the live interactive Git diff, patch preview, and persistent status line. **Accepted at `8fea9feb...`.**
5. Add real SSE/provider streaming behind interactive detection, keep the blocking path intact, and VAX-validate the SSE parser plus DEC C `popen()` and live provider behavior. **In progress.**
6. Reconcile real provider usage into token metrics after transport acceptance; never synthesize unavailable monetary cost.
