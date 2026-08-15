# M259 Phase 8 Image Input Audit

**Baseline:** canonical `main` merge `8f1d1f396091377dff50505716de1dd1adcb1f07` (M258 post-merge validated)

M258 / Phase 7 is complete on canonical main. The remaining substantive parity capability is CAP-026 image input; CP-020 remains MISSING because its combined acceptance condition requires image input plus the already-verified external-tool interface.

## Authoritative Phase 8 target

Phase 8 requires:

1. define supported OpenVMS image file formats and size limits;
2. add a safe upload or local-file ingestion path;
3. include image references in agent requests;
4. record image metadata without placing binary data in text logs; and
5. add image-assisted coding acceptance tests.

## Existing request-path audit

`SRC/OPENAI_REQUEST.C` calls the text-only `write_request(model, prompt, previous_id)` path before invoking curl against `/v1/responses`.

`SRC/OPENAI_REQUEST_BASIC.C` serializes `input` as a single JSON string.

`SRC/OPENAI_REQUEST_AGENT.C` likewise serializes the initial agent user prompt as a single JSON string; only function-call output continuations use structured `input` arrays.

No existing production image-input parser, binary reader, base64 encoder, MIME detector, or `input_image` request serializer was found.

## Responses API representation

The current Responses API accepts a user message whose content array includes `input_text` and `input_image`. `input_image.image_url` may be a fully qualified URL or a base64-encoded data URL.

For OpenVMS local-file parity, M259 will use a base64 data URL so image bytes do not require a separate upload service or persistent remote file object.

## M259 implementation direction

The initial supported local image types are intentionally narrow and inspectable:

- PNG (`image/png`)
- JPEG (`image/jpeg`)
- GIF (`image/gif`)
- WEBP (`image/webp`)

M259 will reject unsupported extensions and files outside the guarded project path.

A bounded image size will be enforced before base64 encoding. The initial implementation target is 5 MiB per image; this is an OVMS Agent safety limit, not a claim about the upstream API maximum.

Image request serialization will use a structured user message equivalent to:

```json
{
  "role": "user",
  "content": [
    {"type": "input_text", "text": "..."},
    {"type": "input_image", "image_url": "data:image/png;base64,...", "detail": "auto"}
  ]
}
```

The request temporary file may contain the transient base64 payload because it is the API body, but persistent activity/session logs must contain only metadata such as guarded path, MIME type, byte count, and outcome. Binary/base64 image content must never be written to those logs.

## Planned evidence slices

### M259.1 - foundation audit

- capture the current text-only request boundary;
- define formats, path policy, size bound, and request representation;
- add focused build driver without production behavior changes.

### M259.2 - local image ingestion

- safe guarded path resolution;
- extension/MIME validation;
- binary size enforcement;
- base64 encoding with deterministic regression fixtures;
- metadata-only reporting.

### M259.3 - Responses request integration

- structured `input_text` + `input_image` request serialization;
- preserve normal text-only behavior when no image is supplied;
- prove image bytes are present only in the temporary API request body;
- prove persistent logs contain metadata only.

### M259.4 - agent workflow acceptance

- expose an explicit image-assisted agent command/workflow;
- prove the selected image reaches the production request-builder boundary with the coding goal;
- add success and refusal evidence without requiring a live network call.

## Compatibility rule

OpenVMS V7.2 VAX / DEC C remains a target. Every newly introduced linker-visible identifier must be 31 characters or fewer. Prefer `static` helpers and audit symbol lengths before every M259 package.

M259.1 introduces no production linker-visible symbols.
