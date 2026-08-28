# Security Policy

OVMS Agent can read source trees, call external AI services, write project files under guarded conditions, run selected OpenVMS commands, and perform authenticated GitHub operations. Its security model therefore depends on both credential hygiene and preserving the approval/write/network boundaries built into the program.

## Security-sensitive data

Never commit or publish:

- AI provider API keys;
- GitHub personal access tokens;
- passwords or private authentication material;
- temporary authorization-header files;
- provider/GitHub profile files from `SYS$LOGIN`;
- logs or fixtures containing live secrets.

OVMS Agent intentionally stores user credentials outside the repository checkout.

Provider profiles are stored in:

```text
SYS$LOGIN:OVMS_AGENT_CONFIG.DAT
```

Ordinary persistent settings are stored in:

```text
SYS$LOGIN:OVMS_AGENT_SETTINGS.DAT
```

GitHub profiles are stored in:

```text
SYS$LOGIN:OVMS_AGENT_GITHUB.DAT
```

Access keys and tokens are entered without normal terminal echo and are masked in normal command output. These files should retain restrictive OpenVMS protections appropriate to their contents.

## Approval and execution boundaries

OVMS Agent separates several kinds of authority instead of treating one switch as permission for everything.

Important boundaries include:

- session approval policy (`read-only`, `workspace`, `full`);
- the guarded-write gate;
- the DCL-execution gate;
- network allow/deny policy;
- autonomous turn/write ceilings;
- the AUTOPILOT safety floor;
- FULL-only external/GitHub operations.

Enabling one gate does not bypass the others.

Use `read-only` for analysis whenever possible. Use `workspace` for intentional controlled project changes. Use `full` only when an operation explicitly requires the highest policy tier.

Do not leave guarded writes or DCL execution enabled merely for convenience when the current task does not need them.

## AUTOPILOT safety floor

`AGENT/AUTOPILOT` is intentionally limited to bounded local workspace repair/build behavior.

It must not automatically satisfy FULL-gated operations. In particular, AUTOPILOT does not authorize arbitrary DCL, GitHub writes, PUSH, MCP/external actions, or other FULL-only work.

Autonomous runs also retain hard turn/write ceilings and must stop in explicit terminal states rather than silently continuing without bounds.

Changes that weaken these floors should be treated as security-sensitive architecture changes and require explicit review, regression coverage, and documentation updates.

## DCL execution

Direct DCL execution is a privileged capability. The DCL gate and approval policy exist to keep ordinary source inspection or editing from silently becoming command execution.

Prefer dedicated read/project commands over DCL when a narrower operation exists.

Do not broaden DCL allow/deny behavior casually. Changes to command filtering, approval requirements, command-procedure handling, chaining/redirection handling, or execution logging should receive focused security review.

## Guarded writes and transactions

File-changing workflows should use the established guarded write and transaction primitives rather than introducing a parallel writer.

The existing design provides bounded project scope, confirmation/approval checks, exact change accounting, and rollback behavior for incomplete automatic multi-file work.

Security-sensitive write-path changes include:

- bypassing project-root checks;
- bypassing confirmation/approval;
- losing rollback guarantees;
- writing through a path that damages OpenVMS RMS record semantics;
- creating files outside the approved workspace without explicit FULL-gated design.

## GitHub authentication

Saved GitHub profiles contain repository/user/branch information and a token. Authenticated Git network operations on OpenVMS use short-lived repository-scoped configuration through the `GITAUTH` wrapper.

Do not replace this with plaintext global credential storage.

The shipped GitHub bridge creates short-lived authorization-header, JSON body, and API-response files as needed. Credential-bearing temporary files are protected against group/world access and removed across OpenVMS file versions after use.

A checkout/profile repository mismatch must fail closed rather than send credentials to the wrong repository.

PUSH and GitHub service mutations remain protected by FULL approval.

## Provider endpoints and intermediaries

A provider profile may point directly to an AI provider or to an OpenAI-compatible intermediary such as Requesty.

Treat endpoint changes as trust-boundary changes. Before using a new endpoint, understand which service receives prompts, source excerpts, tool results, and credentials.

Do not hard-code provider secrets or endpoint-specific assumptions into generic source paths when profiles/settings already provide the appropriate boundary.

## Network policy

Use `net_allow` and `net_deny` when outbound access must be constrained.

A network-capable feature should not silently create an alternate transport that bypasses the existing network policy, approval checks, or credential handling.

## Dependency and transport changes

Changes involving HTTPS, `curl`, provider request/response handling, SSE/stream transport, Git, or external service bridges can alter security boundaries even when the visible command syntax does not change.

Review such changes for:

- credential exposure in command lines or logs;
- insecure temporary files;
- unintended redirects or destination changes;
- repository/profile mismatches;
- unsafe shell/DCL quoting;
- failure to remove temporary files across RMS versions;
- provider-specific assumptions leaking into generic paths.

## Reporting a vulnerability

Do not post live credentials, exploit payloads containing secrets, or unnecessarily detailed private-environment information in a public issue.

If GitHub private vulnerability reporting or a repository Security Advisory channel is available, use that for sensitive reports. If no private reporting channel is available, open a minimal public issue stating that you have a security concern and request a private contact path without including sensitive exploit details.

For non-sensitive hardening defects that do not expose users or secrets, a normal GitHub issue is appropriate.

A useful report includes:

- affected commit/tag/version;
- affected command or subsystem;
- required approval/settings state;
- impact;
- minimal reproduction steps with secrets removed;
- whether the issue is specific to VAX/OpenVMS, RMS behavior, Git integration, or a provider/GitHub endpoint.

## Supported versions

Security fixes are expected to land on the current maintained `main` line first unless a separate supported release/backport policy is explicitly established.

Permanent milestone tags provide historical reference points, but the existence of a tag does not imply indefinite security maintenance for that milestone.

## Security documentation responsibility

When a change materially alters credentials, approvals, network boundaries, DCL execution, guarded writes, GitHub authentication, autonomous safety floors, or temporary sensitive data handling, update this file in the same milestone.