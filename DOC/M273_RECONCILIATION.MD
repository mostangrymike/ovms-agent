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

## Status and provider usage

The interactive agent loop shows the current turn against the active `AGENT/AUTO/LIMITS` ceiling while a provider request is in flight. The first streamed text delta clears that status line once before incremental text is written; later deltas do not erase earlier streamed text. Tool-only responses clear the status after the provider request completes.

M273 records only provider-supplied Responses `usage` values. `input_tokens`, `output_tokens`, and `total_tokens` are parsed from the completed response object, retained as the latest response plus current-process cumulative totals, and appended to `AGENT/METRICS`. The same state supplies compact token text to the already VAX-accepted ANSI status-line renderer. Missing usage is reported as unavailable rather than inferred.

The Phase 6 live VAX gate at `d44424fae4d9b8f4a45048ab0018d8fa8e5a40de` used a fresh clean GITAUTH clone and passed the full canonical build with `BUILD_STATUS=%X00000001`. A live Requesty ASK then produced provider usage `input=15 output=206 total=221`; `AGENT/METRICS` reported those exact latest values, `Responses with usage: 1`, and matching current-process cumulative totals. A subsequent read-only AGENT request remained functional. Monetary cost remained explicitly unavailable because no trusted provider-supplied cost was captured.

The token status text is transient by design and is erased before ordinary output, so pasted scrollback is not expected to retain it. Its renderer is the same `ansi_term_status()` path accepted in the Phase 4 visual gate; Phase 6 changes only the formatted text passed to that renderer, and the canonical VAX regression validates that formatter/accounting path.

M273 does not estimate monetary cost from model names or external price tables. Cost may be added later only if a trusted provider response exposes authoritative cost data or a separately governed pricing source is introduced.

## Streaming is a networking capability

The original provider transport invoked `curl` with `--output LLM_RESPONSE_FILE` through `system()`, so the parent waited for a complete response before parsing it. Merely printing that completed response in chunks would not be streaming and was intentionally rejected.

M273 adds a separate incremental path while preserving that original blocking path as the compatibility contract:

1. Streaming is considered only when `ANSI_TERM` has positively identified a real interactive terminal. Non-terminal/plain sessions continue through the original blocking request path.
2. `OVMS_AGENT_NOSTREAM` independently disables streaming without disabling ANSI colors or the status line.
3. A temporary copy of the already-built JSON request receives `"stream":true`; the original request file is left unchanged.
4. The transport starts `curl --no-buffer` with DEC C `popen()` and reads the child stream incrementally.
5. `SRC/LLM_SSE.C` parses Responses SSE data and passes `response.output_text.delta` text to `ansi_term_stream()` immediately.
6. `response.completed`, `response.failed`, or `response.incomplete` supplies a nested final `response` object. The parser writes that object back as the ordinary `LLM_RESPONSE_FILE`, preserving existing response-id, tool-call, context, final-response, and usage parsing.
7. If a provider returns ordinary JSON instead of recognized SSE, the compatibility path retains completed-response behavior rather than inventing incremental text.
8. If streaming cannot start, or ends unrecognized before any assistant delta has been displayed, the transport may retry through the original blocking path. Once assistant text has actually streamed, it never silently resends the request because that could duplicate both output and provider actions/cost.

The first live Requesty gate proved that the provider and request format remained healthy: `OVMS_AGENT_NOSTREAM` returned a normal blocking response. A raw response-body diagnostic then proved Requesty was returning the expected Responses SSE event family, including `response.created`, `response.in_progress`, `response.output_item.added`, `response.content_part.added`, and `response.output_text.delta` records.

OpenVMS C RTL `popen()` is record-oriented by default. A candidate using `DECC$POPEN_NO_CRLF_REC_ATTR=ENABLE` was rejected after a live Requesty run stopped making progress and had to be interrupted. M273 therefore does not use that feature logical as a launcher or runtime dependency.

A fresh VAX clone of `8ca485680697d459b149385f9a13a615f2ce6f59` passed the full canonical build with `BUILD_STATUS=%X00000001`, but the live Requesty 20-line ASK still displayed only after the request completed. That exposed two separate problems: the prior regression proved only eventual child output, and OpenVMS record boundaries could split SSE protocol tokens themselves.

M273 therefore uses the C RTL's separate `DECC$STREAM_PIPE=ENABLE` feature, scoped with `DEFINE/USER` immediately before the canonical OVMS Agent image activation and immediately before the focused stream regression. The focused child/sentinel regression requires the first child marker to become readable before a delayed sentinel is created, proving incremental parent delivery rather than eventual EOF delivery.

The live raw capture then showed SSE tokens fragmented across OpenVMS pipe records, including splits inside `event:`, `data:`, and `response.output_text.delta`. `SRC/LLM_SSE.C` was consequently changed from line-oriented parsing to a byte-stream state machine that tolerates arbitrary record fragmentation, finds `data:` across record boundaries, reconstructs balanced JSON objects, and uses each object's JSON `type` as the authoritative event type. The regression writes an SSE fixture with a record boundary after every character and requires exact streamed text plus final completed-response reconstruction.

The resulting streaming head `1667dbcd2cf9dddc7441bd1f5f3ed7e08d5430c9` was VAX-accepted from a fresh clean clone with the full canonical build green. Live Requesty 20-line and sustained 100-line ASK tests then displayed incrementally without the `M273 stream was not recognized; retrying blocking transport.` fallback diagnostic. That completes the real provider/network streaming gate.

`OVMS_AGENT_STREAM_DEBUG` remains a diagnostic escape hatch. When defined, it writes only content read from curl stdout to `OVMS_AGENT_STREAM_RAW.TMP`; it does not write the Authorization header, API key, or request body.

AGENT/PLAN intentionally remains completed-response presentation in M273 because its final response text is also persisted as the plan artifact. Other ordinary response/final-answer paths suppress their completed-text print when deltas were already displayed, preventing duplicate assistant output.

## M273 sequencing

1. VAX-validate the isolated ANSI module and plain-output invariants. **Accepted.**
2. Wire the module and focused no-ESC regressions into the canonical build. **Accepted.**
3. Route diff/approval, assistant-response, and turn-status presentation without changing write/approval semantics. **Accepted.**
4. Validate the live interactive Git diff, patch preview, and persistent status line. **Accepted at `8fea9feb...`.**
5. Add real SSE/provider streaming behind interactive detection, keep the blocking path intact, and VAX-validate the SSE parser, DEC C `popen()`, and live provider behavior. **Accepted at `1667dbcd...`.**
6. Reconcile real provider usage into token metrics/status after transport acceptance; never synthesize unavailable monetary cost. **Accepted at `d44424fa...`.**

M273 code acceptance head is `d44424fae4d9b8f4a45048ab0018d8fa8e5a40de`. Any later reconciliation-only commit does not supersede that VAX-tested code checkpoint.
