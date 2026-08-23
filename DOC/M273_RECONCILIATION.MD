# M273 ANSI Presentation Reconciliation

## Scope

M273 introduces a self-contained ANSI terminal-presentation layer for OVMS Agent. Presentation is deliberately separated from write, approval, parsing, networking, and provider control flow so the feature can be bypassed or removed without changing project semantics.

## ANSI instead of SMG$

The project does not need a full-screen terminal application. The visible goals are bounded: colorized diff lines, incremental assistant text when transport eventually supports it, and one carriage-return/repaint status line. Standard SGR and erase-line sequences are sufficient for VT-series terminals and DECterm-style environments without adding an SMG$ screen-management dependency or a multi-pane TUI architecture.

ANSI emission is therefore centralized in `SRC/ANSI_TERM.C`. Callers should request presentation operations from this module rather than embedding escape sequences themselves.

## Plain-output safety rule

ANSI output is opt-in by positive capability detection, not opt-out by assumption. `ANSI_TERM` asks OpenVMS for the `DVI$_DEVCLASS` of `SYS$OUTPUT` and enables ANSI only when OpenVMS identifies it as `DC$_TERM`. This follows the documented OpenVMS test for distinguishing a terminal from redirected/non-terminal output. Redirected files, pipes/mailboxes, batch output, and other non-terminal devices remain plain.

An earlier M273 candidate additionally gated terminal output on `DEV$M_ODV` and `DEV$M_DET`. VAX probing showed that this was too restrictive for the live interactive environment, so those extra characteristic gates were removed rather than teaching the presentation layer a platform-specific false negative.

`OVMS_AGENT_PLAIN` and `OVMS_AGENT_NOCOLOR` are explicit logical-name escape hatches. If either is defined, plain mode wins before terminal detection is consulted.

Persistent status-line operations are a no-op in plain mode so existing batch and machine-driven output does not gain new lines.

## Streaming is not presentation-only

The current provider transport is blocking. `SRC/LLM_TRANSPORT.C` invokes `curl` with `--output` to write a complete `LLM_RESPONSE_FILE`; callers parse that file only after the request process exits. There is therefore no sequence of partial model text for the terminal layer to display today.

True streaming requires a transport change first: request the provider's streaming/SSE response mode, consume the response incrementally, parse event/data records without waiting for a complete JSON response file, and preserve the existing non-streaming path for providers or workflows that cannot stream. Only after that transport contract exists should `ansi_term_stream()` be fed partial assistant text.

This dependency is intentionally recorded here so future work does not simulate streaming by merely printing an already-complete response in chunks.

## M273 sequencing

1. VAX-validate the isolated ANSI module and plain-output invariants.
2. Wire the module into the canonical build.
3. Route existing diff/approval, assistant-response, and status presentation call sites through it without changing their control flow.
4. Add the interactive one-line turn/metrics status display.
5. Treat SSE/provider streaming as a networking sub-slice, with its own transport acceptance, before enabling incremental terminal display.
6. Run focused redirected/override regressions plus the full OpenVMS/VAX build before promotion.
