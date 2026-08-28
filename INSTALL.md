# Installing OVMS Agent

OVMS Agent is a native OpenVMS application. The normal installation model is to place a repository checkout in an OpenVMS directory, build it with the supplied DCL procedure, and launch the resulting image with `OVMS_AGENT.COM`.

## Prerequisites

You need:

- OpenVMS.
- A C compiler suitable for the target OpenVMS system. The project is developed with DEC C/VSI C compatibility in mind and preserves historical VAX/DEC C constraints where they affect source and linker behavior.
- TCP/IP networking for network-backed AI and GitHub features.
- HTTPS capability for provider and GitHub access. The shipped GitHub service bridge uses `curl`.
- Git is strongly recommended. The project uses Git for repository context, guarded source work, milestone control, and GitHub integration.

For the VSI Git environment used during project development, reliable HTTPS Git operation may also require:

```text
$ SET PROCESS/PARSE_STYLE=EXTENDED
$ SET PROCESS/PRIVILEGE=SHARE
```

`SHARE` must already be an authorized privilege for the account.

## Obtain the source

For a public checkout, clone the repository with the Git installation available on your OpenVMS system, then set your default directory to the checkout. For example:

```text
$ git clone https://github.com/mostangrymike/ovms-agent.git
$ SET DEFAULT [.OVMS-AGENT]
```

OpenVMS Git/RMS behavior varies by Git version and target architecture. If an existing accepted OVMS Agent checkout is being updated rather than installed fresh, follow the RMS-aware workflow documented in `CONTRIBUTING.md` instead of assuming a Unix-style reset or checkout will materialize files correctly.

## Build

Run the project build procedure from the repository root:

```text
$ @BUILD
```

A successful build ends with:

```text
Building OVMS Agent Version 2...
All regression tests passed.
Build completed successfully.
Run with: $ @OVMS_AGENT
```

The executable is placed under `[.BUILD]`. Do not move individual object files or bypass the supplied build procedures unless you are deliberately working on the build system itself.

For build internals and diagnostic options, see `BUILD.md`.

## Start OVMS Agent

Launch it from the repository root:

```text
$ @OVMS_AGENT
```

`OVMS_AGENT.COM` resolves the executable relative to its own checkout, verifies that the built image exists, supplies the shipped GitHub service bridge when no explicit bridge override is already defined, enables the stream-pipe behavior needed by the network transport for the image activation, and then runs OVMS Agent.

Exit the interactive program with:

```text
QUIT
```

## Configure an AI provider

Provider profiles are stored outside the repository in:

```text
SYS$LOGIN:OVMS_AGENT_CONFIG.DAT
```

Inside OVMS Agent, create and select a provider profile with:

```text
PROVIDER ADD name
PROVIDER LIST
PROVIDER USE name
MODEL
```

`PROVIDER ADD` prompts for the service address, model, and access key. Access keys are entered without terminal echo and are not intended to be committed to the repository.

The configured endpoint may be a direct provider or an OpenAI-compatible intermediary such as Requesty. Normal provider/model changes do not require rebuilding OVMS Agent.

## Configure normal settings

Run:

```text
SETTINGS
```

Ordinary persistent settings are stored in:

```text
SYS$LOGIN:OVMS_AGENT_SETTINGS.DAT
```

For the complete settings reference, including when each setting should be changed, use the root-level `OVMS_AGENT.HLP` source through an OpenVMS HELP library or consult its corresponding topics.

## Optional GitHub integration

GitHub profiles are stored outside the repository in:

```text
SYS$LOGIN:OVMS_AGENT_GITHUB.DAT
```

Inside OVMS Agent, configure a profile with:

```text
GITHUB ADD name
GITHUB LIST
GITHUB USE name
AGENT/GITHUB/CHECK
```

After a working GitHub profile exists, install the authenticated raw-Git wrapper used by the established OpenVMS workflow:

```text
$ @GIT_AUTH_SETUP
```

A successful setup installs the wrapper under `SYS$LOGIN`, normalizes local `core.filemode` behavior for OpenVMS, and defines the DCL command:

```text
GITAUTH
```

For later logins, after normal Git setup, load the installed wrapper with:

```text
$ @SYS$LOGIN:OVMS_AGENT_GIT_AUTH.COM
```

Use `GITAUTH` for GitHub network commands that need the active saved OVMS Agent GitHub profile. Do not replace this mechanism with a plaintext credential helper.

## First checks

Useful first commands are:

```text
HELP
SETTINGS
ROOT
TREE
STATUS
PROVIDER SHOW
MODEL
AGENT/GITHUB/CHECK
```

For detailed command semantics and practical usage guidance, see `OVMS_AGENT.HLP`.

## Upgrading

Treat an upgrade as a source-control operation, not as an in-place replacement of random built files:

1. Preserve your `SYS$LOGIN` provider/settings/GitHub profile files.
2. Update the repository checkout.
3. Materialize changed tracked files using the RMS-aware procedure appropriate to your environment.
4. Run `@BUILD` when source/build inputs changed.
5. Launch with `@OVMS_AGENT`.

Generated `[.BUILD]` artifacts are disposable. Persistent user configuration is intentionally kept outside the repository checkout.