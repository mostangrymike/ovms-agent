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

CAP-025 remains MISSING because there is no reconciled configurable outbound network policy.
CAP-027 remains MISSING because existing external-tool work has not yet been reconciled against the complete Phase 7 acceptance requirements.

## Existing MCP/external-tool foundation

The repository already contains substantial external-tool functionality.

### M241 - server catalog

`M241_MCP_CATALOG_TEST` proves configured MCP server inventory, case-insensitive lookup, invalid-entry rejection, and empty-configuration behavior for stdio and HTTP server definitions.

### M242 - guarded call parsing

The existing M242 regression covers the guarded MCP call interface before execution.

### M243 - stdio execution

`M243_MCP_EXEC_TEST` proves:

- FULL approval is required before execution;
- configured stdio bridges can execute an explicitly named tool;
- unsafe stdio bridge targets are refused;
- unsupported transports are refused at that layer; and
- executor failures are normalized and reported.

### M244 - HTTP transport

`M244_MCP_HTTP_TEST` proves:

- FULL approval gating;
- HTTPS endpoint validation;
- HTTP execution through an injected transport executor;
- compatibility with stdio transport; and
- normalized HTTP failure reporting.

### M245 - SSE transport

`M245_MCP_SSE_TEST` proves:

- FULL approval gating;
- HTTPS SSE endpoint validation;
- SSE execution through an injected transport executor;
- HTTP and stdio compatibility; and
- normalized SSE failure reporting.

### M246 - normalized result model

`M246_MCP_RESULT_TEST` proves a structured external-tool result carrying server, transport, tool, status, and bounded detail text across success, failure, refusal, and unsafe-endpoint cases.

### M247 - agent feedback and catalog exposure

The external parity catalog registers `mcp_call` as an external/full-approval tool, and M247 provides agent-facing result/feedback evidence.

## Confirmed Phase 7 gaps

### No explicit outbound network policy

HTTP/SSE endpoint validation currently protects transport shape (for example HTTPS versus `file://`) but does not provide a configurable default-deny domain policy with explicit allow and deny lists.

### No policy-exception approval model

FULL agent approval is necessary for MCP execution, but that is not the same as approving a network-policy exception for a destination that is not normally allowed. Phase 7 requires those decisions to be separately inspectable.

### Lifecycle/timeout/output bounds need reconciliation

The MCP result structure already bounds normalized detail text, but Phase 7 still needs explicit evidence for timeout behavior, transport lifecycle/termination, output-size enforcement at the execution boundary, and preservation/reporting of OpenVMS condition status where applicable.

### Policy-decision logging needs evidence

Existing activity logging is mature, but Phase 7 requires reproducible evidence that network allow/deny/exception decisions and external-tool execution outcomes are logged.

## M258 implementation direction

Preserve the proven M241-M247 external-tool stack and add the missing policy/lifecycle layer rather than creating a second external-tool system.

The smallest safe sequence is:

1. establish M241-M247 as focused M258 baseline evidence;
2. add a default-deny network policy with explicit allow/deny domain lists;
3. require an explicit exception approval for destinations outside the allow policy while never allowing an explicit deny entry to be bypassed silently;
4. route HTTP/SSE MCP transports through that policy before transport execution;
5. define bounded timeout/output/condition-status behavior and deterministic failure results;
6. log every network-policy decision and external-tool terminal outcome;
7. reconcile CAP-025 and CAP-027 only after focused OpenVMS regressions pass.

## Compatibility rule

OpenVMS V7.2 VAX / DEC C remains the target. Every newly introduced externally visible identifier must be 31 characters or fewer before compile/link. Prefer `static` helpers and reuse existing public interfaces where possible.

M258.1 introduces no production C identifiers or linker-visible symbols.
