# M273 ANSI Presentation Reconciliation

## Scope

M273 introduces a self-contained ANSI terminal-presentation layer for OVMS Agent. Presentation is deliberately separated from write, approval, parsing, networking, and provider control flow so the feature can be bypassed or removed without changing project semantics.

## ANSI instead of SMG$

The project does not need a full-screen terminal application. The visible goals are bounded: colorized diff lines, incremental assistant text when transport eventually supports it, and one carriage-return/repaint status line. Standard SGR and erase-line sequences are sufficient for VT-series terminals and DECterm-style environments without adding an SMG$ screen-management dependency or a multi-pane TUI architecture.

ANSI emission is therefore centralized in `SRC/ANSI_TERM.C`. Callers should request presentation operations from this module rather than embedding escape sequences themselves.

## Plain-output safety rule

ANSI output is opt-in by positive capability detection, not opt-out by assumption. `ANSI_TERM` asks OpenVMS for the `DVI$_DEVCLASS` of `SYS$OUTPUT` and enables ANSI only when OpenVMS identifies it as `DC$_TERM`. This follows the documented OpenVMS test for distinguishing a terminal from redirected/non-terminal output. Redirected files, pipes/mailboxes, batch output, and other non-terminal devices remain plain.

An earlier M273 candidate additionally gated terminal output on `DEV$M_ODV` and `DEV$M_DET`. VAX probing showed that this was too restrictive for the live interactive environment, so those extra characteristic gates were removed rather than teaching the presentation layer a platform-specific false negative.

The accepted Phase 0/1 VAX gate reported `SYS$OUTPUT DEVCLASS=66 DEVNAM=_FTA5:` and `ANSI terminal probe: enabled=1 forced_plain=0`. The same exact module passed redirected-output and `OVMS_AGENT_PLAIN` captures with a byte-level verifier that rejects byte `0x1B`; both captures contained no escape byte.

`OVMS_AGENT_PLAIN` and `OVMS_AGENT_NOCOLOR` are explicit logical-name escape hatches. If either is defined, plain mode wins before terminal detection is consulted.

Persistent status-line operations are a no-op in plain mode so existing batch and machine-driven output does not gain new lines.

## Presentation routing

The production integration keeps presentation decisions out of mutation/control-flow code:

- assistant response/error/raw-response display routes through `ANSI_TERM`;
- `GITDIFF` and repair final-diff rendering use an ANSI diff callback registered by `MAIN`, while legacy minimal-link tests retain their original plain fallback when no callback is registered;
- AGENT/WRITE exact-text and line-range Current/Proposed previews route stdout presentation through `ANSI_TERM` while file streams and transaction logic remain ordinary stdio;
- AGENT/PATCH adds an interactive-only colored structured-patch preview after the existing validator succeeds; plain mode adds no preview bytes;
- saved-plan/approval stdout routes through a presentation wrapper without changing approval text or yes/no semantics.

The first integrated VAX interactive check found an OpenVMS Git compatibility issue in the initial colored-diff implementation: a Git invocation using `-c color.diff...` and `--color=always` returned success but emitted no diff even though `git status --short` showed a modified file. M273 therefore no longer depends on Git to emit ANSI. In interactive mode, `ANSI_TERM` asks Git for an ordinary `--no-color` diff in a temporary file, classifies added/deleted/context lines itself, emits SGR locally, and removes all temporary OpenVMS file versions. Plain mode deliberately keeps the pre-M273 `git diff --` command unchanged.

## Status and metrics boundary

The interactive agent loop can show the current turn against the active `AGENT/AUTO/LIMITS` ceiling while a provider request is in flight. The line is erased before response/tool output and is a no-op in plain mode.

The existing `AGENT/METRICS` implementation does not currently collect provider input tokens, output tokens, total tokens, or monetary cost. It summarizes workflow/build/patch/self-test activity from the activity log. M273 therefore must not invent a token or cost number. A future token/cost status field requires provider-usage accounting first; monetary cost additionally requires a trustworthy provider/model pricing source or provider-supplied cost data.

## Streaming is not presentation-only

The current provider transport is blocking. `SRC/LLM_TRANSPORT.C` invokes `curl` with `--output` to write a complete `LLM_RESPONSE_FILE`; callers parse that file only after the request process exits. There is therefore no sequence of partial model text for the terminal layer to display today.

True streaming requires a transport change first: request the provider's streaming/SSE response mode, consume the response incrementally, parse event/data records without waiting for a complete JSON response file, and preserve the existing non-streaming path for providers or workflows that cannot stream. Only after that transport contract exists should `ansi_term_stream()` be fed partial assistant text.

This dependency is intentionally recorded here so future work does not simulate streaming by merely printing an already-complete response in chunks.

## M273 sequencing

1. VAX-validate the isolated ANSI module and plain-output invariants. **Accepted.**
2. Wire the module and focused no-ESC regressions into the canonical build. **Accepted by full VAX build at integration checkpoint `0c1aabb3...`.**
3. Route diff/approval, assistant-response, and turn-status presentation without changing write/approval semantics.
4. Re-run the interactive Git diff/status/color checks after the local diff-rendering compatibility fix.
5. Treat SSE/provider streaming as the networking sub-slice, with its own transport acceptance, before enabling incremental terminal display.
6. Reconcile the token/cost request only after real usage-accounting data exists; never synthesize cost from unavailable metrics.
