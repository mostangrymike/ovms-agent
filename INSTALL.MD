# Installing OVMS Agent

OVMS Agent is a native OpenVMS application. The normal model is to place a repository checkout in an OpenVMS directory, build it with the supplied DCL procedure, configure provider/settings state under `SYS$LOGIN`, and launch it with `OVMS_AGENT.COM`.

## Current validated environment

The current canonical project acceptance host is **OpenVMS V9.2-3 x86-64**. The source preserves historical DEC C/VAX compatibility constraints where they affect source/linker behavior, but VAX is not the current canonical acceptance system.

## Prerequisites

You need:

- OpenVMS;
- a C compiler suitable for the target OpenVMS system;
- TCP/IP networking for network-backed AI/GitHub features;
- HTTPS/curl capability for provider and GitHub service access; and
- Git, strongly recommended for repository context, checkpoints, and GitHub integration.

For the VSI Git environment used during development, reliable HTTPS Git operation may require:

```text
$ SET PROCESS/PARSE_STYLE=EXTENDED
$ SET PROCESS/PRIVILEGE=SHARE
```

`SHARE` must already be an authorized privilege for the account.

## Obtain the source

For a public checkout, clone the repository with the Git installation available on OpenVMS and set default to the checkout. For a stable public release, use the corresponding semantic tag such as `v1.0.0`.

When updating an existing accepted checkout, do not assume Unix-style reset/checkout semantics will safely reconstruct RMS-visible files. Follow `DOC/OPENVMS_GIT_QUIRKS.MD` and `CONTRIBUTING.MD`.

## Build

From the repository root:

```text
$ @BUILD
```

A successful 1.0.0 build ends with:

```text
Building OVMS Agent Version 1.0.0...
All regression tests passed.
Build completed successfully.
Run with: $ @OVMS_AGENT
```

The executable is placed under `[.BUILD]`. See `BUILD.MD` for build/acceptance details.

## Start OVMS Agent

```text
$ @OVMS_AGENT
```

`OVMS_AGENT.COM` resolves the checkout/build image, exposes the shipped GitHub bridge and language directory when no explicit override is present, enables the network stream-pipe behavior required by the transport, and runs the image.

For release verification:

```text
VERSION
```

The 1.0.0 release reports `OVMS Agent 1.0.0`.

## Configure an AI provider

Provider profiles are stored outside the repository in:

```text
SYS$LOGIN:OVMS_AGENT_CONFIG.DAT
```

Inside OVMS Agent:

```text
PROVIDER ADD name
PROVIDER LIST
PROVIDER USE name
MODEL
```

`PROVIDER ADD` prompts for service address, model, and access key; the key is entered without normal terminal echo. The endpoint may be a direct provider or an OpenAI-compatible intermediary such as Requesty.

After configuration, test response health without changing the active profile:

```text
PROVIDER TEST
```

or one profile:

```text
PROVIDER TEST name
```

`OK` means the configured request reached the endpoint, authentication/request format worked, the response parsed, and nonempty assistant text was returned. Other textual states identify configuration, transport, API, empty-output, or invalid-response failures. The diagnostic makes a real small provider request and is not a tool-call/capability benchmark.

## Configure normal settings

Run:

```text
SETTINGS
```

Ordinary persistent settings are stored in:

```text
SYS$LOGIN:OVMS_AGENT_SETTINGS.DAT
```

`OVMS_AGENT.HLP` is the authoritative command/SETTINGS reference.

## Optional GitHub integration

GitHub profiles are stored in:

```text
SYS$LOGIN:OVMS_AGENT_GITHUB.DAT
```

Configure one with:

```text
GITHUB ADD name
GITHUB LIST
GITHUB USE name
AGENT/GITHUB/CHECK
```

Install the authenticated raw-Git wrapper from an accepted checkout:

```text
$ @GIT_AUTH_SETUP
```

For later logins:

```text
$ @SYS$LOGIN:OVMS_AGENT_GIT_AUTH.COM
```

Use `GITAUTH` for authenticated GitHub network commands. Operator-facing:

```text
$ GITAUTH --check
```

performs a live authenticated **write-authorization** preflight using a non-mutating dry-run push against the active profile/current origin. The setup procedure itself uses a config-only check so installation does not require the network to be reachable at that moment.

Do not replace this mechanism with plaintext credential-helper storage.

## First checks

```text
HELP
VERSION
SETTINGS
ROOT
TREE
STATUS
PROVIDER SHOW
PROVIDER TEST
MODEL
AGENT/GITHUB/CHECK
```

If GitHub raw-Git access is configured, also run `GITAUTH --check` from the intended repository before relying on write access.

## Upgrading

1. Preserve the `SYS$LOGIN` provider/settings/GitHub profile files.
2. Update the repository or select the desired release tag.
3. Establish the exact fetched ref/index and materialize changed files using the documented RMS-aware workflow when necessary.
4. Run `@BUILD` when source/build/runtime inputs changed.
5. Launch with `@OVMS_AGENT` and verify `VERSION`.

Generated `[.BUILD]` artifacts are disposable; persistent user configuration is intentionally kept outside the checkout.
