# M300 — Blocking-default LLM transport with opt-in streaming

## Goal

M300 changes OVMS Agent's normal provider transport policy from streaming-first to bounded blocking-first. SSE streaming remains available as an explicit interactive presentation option rather than the default execution path.

## Motivation

M273 proved true incremental SSE delivery on OpenVMS, but the benefit is primarily perceived latency for human-readable text. OVMS Agent's tool execution model still consumes the completed Responses object before tool work proceeds.

Later native evidence showed a recurring compatibility pattern:

- M294: Groq `openai/gpt-oss-*` tool requests failed through streaming and succeeded through bounded blocking Responses transport.
- M295: Groq `qwen/qwen3.6-27b` tool requests showed the same discriminator.
- M296: OpenRouter exact `openrouter/free` tool requests showed the same discriminator.
- M299 validation exposed issue #179: an ordinary ASK could emit valid streamed assistant text and then report a streaming child-completion/status failure.

The accumulating provider/model exception matrix made streaming complexity part of normal operation even though tool-bearing workflows gain no incremental execution benefit from it.

## M300 policy

Persistent setting:

```text
streaming=OFF
```

The setting lives in `SYS$LOGIN:OVMS_AGENT_SETTINGS.DAT` and defaults to `OFF`.

Supported command forms:

```text
SETTINGS GET streaming
SETTINGS SET streaming ON
SETTINGS SET streaming OFF
SETTINGS TOGGLE streaming
```

No new logical enables streaming. The existing `OVMS_AGENT_NOSTREAM` logical remains a compatibility/debug force-blocking override for now. `OVMS_AGENT_STREAM_DEBUG` remains diagnostic only.

Transport selection:

1. If `streaming=OFF`, use bounded blocking Responses transport.
2. If `OVMS_AGENT_NOSTREAM` is defined, force blocking regardless of the saved setting.
3. If the serialized request contains tool definitions, use blocking regardless of provider/model and regardless of `streaming=ON`.
4. If output is noninteractive/plain or the workflow is completed-object-only such as `AGENT/PLAN`, do not incrementally display SSE text.
5. Otherwise, with `streaming=ON`, the existing M273 SSE path may provide incremental text presentation.

M300 retains the proven byte-oriented SSE parser and stream-pipe regression. It changes policy, not the SSE wire parser.

## Source shape

Runtime/source checkpoint changed:

- `BUILD_M273_STREAM.COM`
- `SRC/COMMAND_SETTINGS.C`
- `SRC/LLM_STREAM_POLICY_M300.INC` (new)
- `SRC/LLM_TRANSPORT_M273.C`
- `SRC/M280_SETTINGS_TEST.C`
- `SRC/M300_STREAM_POLICY_TEST.C` (new)
- `SRC/SETTINGS.C`

The generic helper `m300_stream_request_ok()` returns false for tool-bearing serialized requests and true for non-tool request shapes. `LLM_TRANSPORT_M273.C` first checks the persistent setting, then the generic request-shape rule; only an eligible request leaves `OVMS_AGENT_NOSTREAM` effectively undefined for the included transport implementation.

## Native source checkpoint

Exact source checkpoint:

- SHA: `576ce40727648158ddedf41000ed490788b8b94c`
- tree: `7d530fad4f09d0d2f288d2e623ca98bb3b7b494e`
- canonical host: OpenVMS V9.2-3 x86-64

Native evidence:

- exact network/ref identity verified;
- seven changed source/build paths RMS-materialized and physically checked;
- full `@BUILD` passed;
- `All regression tests passed.`;
- `Build completed successfully.`;
- `BUILD_STATUS = "%X00000001"`;
- focused `M300_STREAM_POLICY_TEST` passed with `%X00000001`;
- focused `M280_SETTINGS_TEST` passed with `%X00000001`.

This establishes the source/runtime shape before the HELP/living-document layer. Final acceptance additionally requires HELP library creation/lookup plus live default-blocking, optional-streaming, and tool-bearing blocking gates from the final candidate.

## Issue #179 boundary

M300 intentionally makes #179 non-impacting to default operation by making blocking the default. It does not declare #179 fixed unless the optional streaming live gate proves the child-completion/status defect is actually corrected. If `streaming=ON` still reproduces #179, M300 may still satisfy its default-transport goal while #179 remains open for the optional path, subject to the final acceptance decision recorded in issue #182.
