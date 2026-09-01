# Security Policy

OVMS Agent can read source trees, call external AI services, write project files under guarded conditions, run selected OpenVMS commands, and perform authenticated GitHub operations. Its security model depends on credential hygiene plus independent approval, write, DCL, network, path, transaction, and autonomous-execution boundaries.

## Supported line and environment

The current public release line is `1.0.x`. The current canonical native acceptance host is **OpenVMS V9.2-3 x86-64**. Historical VAX/DEC C compatibility remains relevant to source/linker correctness, but it is not the current security-support host description.

## Security-sensitive data

Never commit or publish:

- provider API keys;
- GitHub personal access tokens;
- passwords/private authentication material;
- temporary authorization headers;
- `SYS$LOGIN` provider/GitHub profile files;
- logs or fixtures containing live secrets.

Persistent stores are intentionally outside the repository:

```text
SYS$LOGIN:OVMS_AGENT_CONFIG.DAT
SYS$LOGIN:OVMS_AGENT_SETTINGS.DAT
SYS$LOGIN:OVMS_AGENT_GITHUB.DAT
```

Keys/tokens are entered without normal terminal echo and masked in normal output. These files should retain restrictive OpenVMS protections.

## Approval and execution boundaries

Independent boundaries include:

- session approval (`read-only`, `workspace`, `full`);
- guarded-write enablement;
- DCL-execution enablement;
- project-root/path and sensitive-file checks;
- network allow/deny policy;
- autonomous turn/write limits;
- AUTOPILOT's inability to satisfy FULL;
- repository/profile matching for authenticated Git; and
- transaction/rollback behavior.

Enabling one boundary does not bypass the others. Prefer `read-only` for analysis, `workspace` for intentional controlled project edits, and `full` only when a feature explicitly requires it.

## AUTOPILOT safety floor

`AGENT/AUTOPILOT` is limited to bounded local workspace repair/build behavior. It cannot satisfy FULL approval and therefore cannot silently authorize arbitrary DCL, PUSH, GitHub mutations, MCP/external actions, or other FULL-gated work.

One invocation consumes a bounded cumulative autonomous budget and must stop in an explicit terminal state with human-review evidence.

## DCL execution

Direct DCL is privileged. Prefer narrower project/read/build tools when possible. Changes to DCL filtering, approval, command-procedure handling, quoting/chaining/redirection, or execution logging are security-sensitive and require focused review/regression evidence.

## Guarded writes and atomic editing

File-changing workflows should reuse established guarded/RMS-aware write primitives.

Ordinary `AGENT/WRITE` existing-file edits preserve a one-materialized-write invariant: `structured_patch` can carry multiple exact non-overlapping hunks for one file through one validation/preview/confirmation/write. Supervised `AGENT/FIX` retains replace primitives; raising `auto_writes` does not permit sequential existing-file patching in ordinary `AGENT/WRITE`.

One-file structured-patch atomicity is not cross-file atomicity. Coordinated multi-file all-or-rollback work belongs to transaction/`CHANGESET` machinery.

Security-sensitive regressions include bypassing root checks/confirmation/approval, weakening rollback, damaging RMS record semantics, or creating a parallel writer outside the established policy path.

## Provider endpoints and provider-health testing

A profile may point directly to a provider or an OpenAI-compatible intermediary such as Requesty. Endpoint changes are trust-boundary changes because prompts, source excerpts, tool results, and credentials are sent to that endpoint.

`PROVIDER TEST` makes a **real** bounded provider request. It must:

- leave the active provider/model unchanged;
- never print the access key or Authorization header;
- use dedicated temporary request/header/response files;
- remove all RMS versions of those temporary files after the probe; and
- remain bounded by provider transport timeout/retry policy.

A green/`OK` result means basic response health only. It does not certify tool-call capability, model safety, or suitability for guarded write workflows.

## GitHub authentication and GITAUTH

Saved GitHub profiles contain repository/user/branch/token information. Raw authenticated Git uses short-lived repository-scoped configuration through `GITAUTH`; interactive credential prompting is disabled.

Do not replace this with plaintext global credential storage.

`GITAUTH --check` is a live authenticated **write-authorization preflight**. It performs a non-mutating dry-run push to the active profile/current origin and therefore catches stale/wrong profile selection and tokens lacking repository write authorization. `GIT_AUTH_SETUP.COM` uses the config-only check so installation is not dependent on current network availability.

The dry-run authorization-check ref must not be created remotely. A successful check should be independently verified when acceptance depends on that fact.

The GitHub service bridge also uses protected short-lived authorization/body/response files and removes them across RMS versions. Checkout/profile repository mismatch must fail closed. PUSH and GitHub service mutations retain FULL approval requirements.

## OpenVMS Git/RMS integrity

Do not treat an odd Git/OpenVMS status, blank/nonblank porcelain, or a successful reset/checkout as sufficient proof of repository state. The observed OpenVMS Git port can expose separate Git metadata and RMS-visible working-file truth.

When acceptance depends on exact state, verify `.git` control files, physical source files, GitHub refs, and native build/runtime evidence independently. Follow `DOC/OPENVMS_GIT_QUIRKS.MD`.

Encoded POSIX-exit statuses must be decoded before applying generic native odd/even reasoning; a numerically odd status can still encode a nonzero POSIX failure.

## Network policy and transport changes

Use `net_allow` / `net_deny` where outbound access must be constrained. New network-capable features should reuse established transport/policy/credential handling rather than creating bypass paths.

Changes involving HTTPS/curl, SSE/response parsing, provider transports, Git, or external bridges must be reviewed for:

- credential leakage in command lines/logs;
- insecure temporary files;
- redirects/destination changes;
- repository/profile mismatch;
- unsafe DCL quoting;
- failure to remove all RMS temp versions;
- unbounded timeout/retry behavior; and
- provider-specific assumptions leaking into generic paths.

## Reporting a vulnerability

Do not post live credentials or unnecessary private-environment data in public issues. Use GitHub private vulnerability reporting/Security Advisories when available. If no private channel is available, open a minimal public issue requesting a private contact path without sensitive exploit detail.

A useful report includes affected commit/tag/version, subsystem/command, required settings/approval state, impact, minimal secret-free reproduction, and whether the behavior is OpenVMS/RMS/Git/provider-specific.

## Security documentation responsibility

When a change materially alters credentials, provider transport, approvals, network boundaries, DCL execution, guarded writes, Git/GitHub authentication, autonomous safety floors, or temporary sensitive-data handling, update this file in the same milestone.
