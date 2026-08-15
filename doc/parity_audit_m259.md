# M259 Phase 8 Image Input Audit

**Baseline:** canonical `main` merge `8f1d1f396091377dff50505716de1dd1adcb1f07` (M258 post-merge validated)

M258 / Phase 7 is complete on canonical main. The remaining substantive parity capability at the start of M259 was CAP-026 image input; CP-020 was MISSING because its combined acceptance condition required image input plus the already-verified external-tool interface.

## Authoritative Phase 8 target

Phase 8 requires:

1. define supported OpenVMS image file formats and size limits;
2. add a safe upload or local-file ingestion path;
3. include image references in agent requests;
4. record image metadata without placing binary data in text logs; and
5. add image-assisted coding acceptance tests.

## Existing request-path audit

Before M259, `SRC/OPENAI_REQUEST.C` called the text-only `write_request(model, prompt, previous_id)` path before invoking curl against `/v1/responses`.

`SRC/OPENAI_REQUEST_BASIC.C` serialized `input` as a single JSON string.

`SRC/OPENAI_REQUEST_AGENT.C` likewise serialized the initial agent user prompt as a single JSON string; only function-call output continuations used structured `input` arrays.

No production image-input parser, binary reader, base64 encoder, MIME detector, or `input_image` request serializer existed before M259.

## Responses API representation

The Responses API accepts a user message whose content array includes `input_text` and `input_image`. `input_image.image_url` may be a base64-encoded data URL.

For OpenVMS local-file parity, M259 uses a base64 data URL so image bytes do not require a separate upload service or persistent remote file object.

## Implemented local-image contract

The supported local image types are intentionally narrow and inspectable:

- PNG (`image/png`)
- JPEG (`image/jpeg`)
- GIF (`image/gif`)
- WEBP (`image/webp`)

M259 rejects unsupported extensions, extension/signature mismatches, unsafe traversal/device-qualified/absolute paths, and files larger than 5 MiB.

The 5 MiB limit is an OVMS Agent safety bound, not a claim about an upstream API maximum.

Base64 is emitted incrementally through fixed-size buffers so the implementation does not require one multi-megabyte encoded image allocation.

Image request serialization uses a structured user message equivalent to:

```json
{
  "role": "user",
  "content": [
    {"type": "input_text", "text": "..."},
    {"type": "input_image", "image_url": "data:image/png;base64,...", "detail": "auto"}
  ]
}
```

The temporary API request file may contain the transient base64 payload because it is the request body. Persistent activity/session logs contain only guarded path, MIME type, byte count, and outcome; binary/base64 image content is not logged.

## M259 evidence

### M259.1 - text-only baseline

`BUILD_M259.COM` proves the pre-existing text-only request path remains unchanged when no image is supplied.

Observed OpenVMS result:

```text
M259 text-only request baseline regression passed.
M259 text-only request baseline evidence passed.
```

### M259.2 - guarded local image ingestion

`SRC/OPENAI_IMAGE.C` plus `SRC/M259_IMAGE_INGEST_TEST.C` prove:

- PNG/JPEG/GIF/WEBP extension and signature validation;
- unsafe path refusal;
- the 5 MiB size limit;
- deterministic base64 data-URL generation; and
- bounded streaming encoding.

Observed OpenVMS result:

```text
M259 guarded image ingestion regression passed.
M259 guarded image-ingestion evidence passed.
```

### M259.3 - production request serialization

The production OpenAI module set includes image support. `write_request_image` emits a structured `input_text` plus `input_image` request while the original `write_request` behavior remains the normal text-only path.

`SRC/M259_REQUEST_IMAGE_TEST.C` proves multimodal serialization, previous-response handling, and refusal without leaving a partial request file.

Observed OpenVMS result:

```text
M259 multimodal request serialization regression passed.
M259 multimodal request serialization evidence passed.
```

The normal `@BUILD` also completed successfully after production integration.

### M259.4 - real agent command and metadata-only logging

M259 exposes:

```text
AGENT/IMAGE image-path goal
```

through the production command registry. The command enters the existing read-only agent loop, retaining the normal registered agent tools. The selected image is attached only to the initial user turn; subsequent tool/result turns use the established request format.

`SRC/M259_AGENT_IMAGE_TEST.C` proves command registration, an image-bearing first agent request with the normal read-only tool schema, and metadata-only activity logging with no base64 payload.

Observed OpenVMS result:

```text
M259 agent image command/logging regression passed.
M259 agent image command/logging evidence passed.
```

The focused driver ended with `%X00000001`.

### M259.5 - live Responses API acceptance

A controlled fixture generator creates `M259_LIVE.PNG` as a 2x2 solid-red RGB PNG.

The first fixture revision produced a malformed 77-byte PNG and the live API correctly rejected it with:

```text
OpenAI API error: The image data you provided does not represent a valid image.
```

That run is fixture-failure evidence only and is not counted as image-input acceptance.

The fixture was corrected in commit `6432ab7` to a verified 73-byte PNG. The generator reopens the created OpenVMS file and performs an exact byte-for-byte readback check before reporting success.

Observed fixture preparation:

```text
Created and verified M259_LIVE.PNG (73 bytes, 2x2 solid red).
Controlled live image is ready: M259_LIVE.PNG
$STATUS == "%X00000001"
```

The real production agent was then invoked as:

```text
OVMS-AGENT> AGENT/IMAGE M259_LIVE.PNG What is the dominant color of this image? Answer with the color and one short sentence.
Project root: SYS$SYSDEVICE:[MIKE.OVMS_AGENT]
Starting read-only agent...
Red.
The image is filled almost entirely with a solid, bright red color.
```

This demonstrates that the locally ingested image was accepted by the live Responses API and reasoned over correctly by the model through the production `AGENT/IMAGE` path.

## Capability reconciliation

### CAP-026 - Image input

**Recommended status: VERIFIED.**

The complete required behavior is now supported by reproducible evidence:

- guarded local image ingestion;
- explicit supported formats and size bound;
- signature validation and unsafe-path refusal;
- production multimodal request serialization;
- real agent command integration;
- metadata-only persistent logging evidence; and
- successful live image reasoning over a controlled PNG.

### CP-020 - Image input and external tools

**Recommended status: VERIFIED.**

M241-M247 and M258 already verify the external-tool half: registered MCP server/tool definitions, FULL approval where required, default-deny network policy, explicit allow/deny and exceptions, stdio/HTTP/SSE bridges, bounded output, condition-status reporting, and logging.

M259 now verifies the missing image-context half through the production `AGENT/IMAGE` workflow and live Responses API acceptance. The image-assisted agent retains the established read-only tool schema on its first request, so image context and the registered agent-tool interface coexist in the same agent request path.

## Phase 8 conclusion

All five Phase 8 roadmap requirements have implementation and OpenVMS evidence. Subject to the matching authoritative `doc/codex_parity.md` status reconciliation and final release-candidate build/regression audit, **Phase 8 is COMPLETE**.

Phase 9 remains the final whole-suite parity validation phase.

## Compatibility rule

OpenVMS V7.2 VAX / DEC C remains a target. Every newly introduced linker-visible identifier must be 31 characters or fewer. Static helpers are preferred and the symbol-length audit must be repeated before M259 packaging and merge.
