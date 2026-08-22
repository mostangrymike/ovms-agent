# M258 Phase 7 Network and External Tools Audit

**Baseline:** `main` merge `ccc2a6b7d7f305ffa7f2563ffdae71465d9a3baf` (M257 post-merge validated)

This audit reconciles Phase 7 / CAP-025 / CAP-027 against the network and MCP/external-tool work already present in the repository.

## Validated baseline

M257 and Phase 6 are complete on canonical `main` at `ccc2a6b`.

Post-merge validation on VSI OpenVMS x86-64 showed:

```text
@BUILD
All regression tests passed.
Build completed successfully.

@BUILD_M257
All M257 project-instruction regressions passed.

$STATUS == "%X00000001"
```

The working tree was clean and synchronized with `origin/main`.

## Authoritative Phase 7 target

Phase 7 requires:

1. network-disabled behavior as the default;
2. explicit domain allow and deny policies;
3. approval for policy exceptions;
4. a registered external-tool schema;
5. lifecycle, timeout, output-size, and condition-status rules;
6. MCP-compatible or OpenVMS-native transport;
7. logging of external-tool and network-policy decisions.

## Existing MCP/external-tool foundation

M241-M247 already provide the registered MCP server/tool surface, guarded call parsing, FULL-approval execution, stdio/HTTP/SSE transport injection, normalized results, and agent/session feedback.

- M241: configured MCP server catalog and invalid-entry handling.
- M242: guarded MCP call parsing.
- M243: FULL-gated stdio execution and unsafe-target refusal.
- M244: HTTP transport execution and endpoint validation.
- M245: SSE transport execution and compatibility with HTTP/stdio.
- M246: normalized server/transport/tool/status/detail result model.
- M247: registered `mcp_call` external tool plus transcript/session evidence.

M258 preserves this stack and adds the missing network-policy and lifecycle layer.

## M258.1 - foundation evidence

The focused `BUILD_M258.COM` driver replays M241-M247 after a successful `@BUILD`.

Validated on OpenVMS:

```text
M258 M241 MCP catalog evidence passed.
M258 M242 guarded-call evidence passed.
M258 M243 stdio execution evidence passed.
M258 M244 HTTP transport evidence passed.
M258 M245 SSE transport evidence passed.
M258 M246 normalized-result evidence passed.
M258 M247 agent-feedback evidence passed.
All implemented M258 foundation regressions passed.
$STATUS == "%X00000001"
```

## M258.2 - default-deny network policy

`SRC/LLM_NETWORK.C` adds:

- default-deny outbound HTTP/SSE policy;
- `OVMS_AGENT_NET_ALLOW` exact and `*.domain` rules;
- `OVMS_AGENT_NET_DENY`, with deny taking precedence over allow;
- FULL-only one-shot network exceptions;
- one-shot exception consumption;
- inspectable policy state; and
- activity-log records for allow, deny, and exception decisions.

`M258_NET_POLICY_TEST` proves all of these semantics.

Validated on OpenVMS:

```text
M258 default-deny network policy regression passed.
M258 default-deny network-policy evidence passed.
$STATUS == "%X00000001"
```

## M258.3 - MCP transport integration

`SRC/LLM_PARITY_M258.C` wraps the mature MCP implementation and applies network policy only after the established FULL-approval and endpoint-syntax checks.

The wrapper preserves stdio behavior and gates HTTP/SSE before executor dispatch.

`M258_MCP_NET_TEST` proves:

- default-denied HTTP/SSE requests do not reach an executor;
- allow-listed HTTP/SSE requests execute;
- explicit deny overrides allow;
- a one-shot exception executes once and is then consumed; and
- stdio remains unaffected.

M244-M246 fixture domains are now explicitly allowed rather than relying on FULL approval as implicit network authorization.

Validated on OpenVMS:

```text
M258 MCP network-policy integration regression passed.
M258 MCP network-policy integration evidence passed.
$STATUS == "%X00000001"
```

## M258.4 - lifecycle, bounds, condition status, and logging

The production bridge contract now includes:

- `timeout_seconds`, default 30, configurable with `OVMS_AGENT_MCP_TIMEOUT_SECONDS`;
- `output_limit`, default 1536 bytes, configurable with `OVMS_AGENT_MCP_OUTPUT_BYTES`;
- bounded bridge response ingestion;
- preservation/reporting of the exact condition value returned by the OpenVMS C RTL `system()` call;
- odd/even condition evaluation using OpenVMS success semantics;
- normalized terminal failure details; and
- activity-log records for terminal MCP outcomes.

Timeout enforcement is explicitly a bridge responsibility. The core passes the configured timeout in every bridge request but does not claim portable forced termination of a hung `system()` subprocess across all supported OpenVMS releases.

`M258_LIFECYCLE_TEST` uses temporary local DCL bridges and no external network access. It proves:

- timeout and output-limit metadata are present in the bridge request;
- successful odd status is preserved;
- an actual even status returned through the C RTL is preserved as an even failure condition;
- oversized bridge responses are refused; and
- the focused test itself returns an even process status on assertion failure.

The first lifecycle run exposed two focused-test issues: missing standalone command-layer stubs and a C `return 1` failure path that OpenVMS correctly interpreted as success. Both were fixed without weakening production behavior.

Corrected final validation:

```text
M258 MCP bridge lifecycle regression passed.
M258 bridge lifecycle/condition/output evidence passed.
M258 timeout enforcement remains bridge-contract responsibility.
All implemented M258 network/external-tool regressions passed.
$STATUS == "%X00000001"
## m258-network-external-tools...origin/m258-network-external-tools
```

The full production `@BUILD` also passed after M258.3/M258.4 production changes.

## Phase 7 reconciliation

All seven Phase 7 requirements now have implementation and focused OpenVMS evidence:

1. default network-disabled behavior - VERIFIED;
2. explicit allow/deny domain policy - VERIFIED;
3. approved policy exceptions - VERIFIED;
4. registered external-tool schema - VERIFIED through M241/M247;
5. lifecycle/timeout/output-size/condition-status rules - VERIFIED, with timeout enforcement assigned to the configured bridge contract;
6. MCP-compatible/OpenVMS-native bridge transport - VERIFIED through stdio/HTTP/SSE evidence;
7. external-tool and network-policy decision logging - VERIFIED.

Therefore:

- CAP-025 Configurable network policy -> VERIFIED.
- CAP-027 External tool extensibility -> VERIFIED.
- Phase 7 -> COMPLETE.
- CAP-026 Image input remains MISSING.
- CP-020 remains MISSING because its combined pass condition requires both image input and external tools.

Practical parity is not yet complete.

## Compatibility audit

OpenVMS V7.2 VAX / DEC C remains the target.

New externally visible identifiers and base aliases introduced by M258 remain within the 31-character VAX linker limit. The longest new alias is:

```text
llm_mcp_exec_transport_base
```

at 30 characters. New M258 helpers are `static` where external linkage is unnecessary.

The final M258 packaging must continue to audit external symbol length and OpenVMS filename case collisions before PR/merge.
