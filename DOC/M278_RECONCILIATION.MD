# M278 Reconciliation — GitHub Service Bridge Completion

## Scope

M278 completes issue #78, the remaining shipped GitHub service-bridge setup, documentation, and service-surface gap.

Base canonical main before M278:
- `9b58ba6e6c8f5337e4bfd284ace47bed26f6cc0e` (promoted M277)

VAX-green M278 code head:
- `5cfee3bb35ba595eae7d8795c54ff323059be088`

Tracking issue:
- #94 `M278 GitHub service bridge completion`

Closed defect:
- #78 `AGENT/GITHUB/PR advertised but no GitHub service bridge implementation or setup is shipped`

Temporary live-acceptance issue:
- #95 `M278-SERVICE-BRIDGE-ACCEPTANCE`, created through the shipped bridge and closed after independent GitHub verification.

## Net code changes

The accepted code head changes exactly these four files relative to promoted M277 main:

- `GITHUB_BRIDGE.COM`
- `OVMS_AGENT.COM`
- `README.md`
- `SRC/m250_github_test.c`

No build or linker wiring changed, and no new external C identifiers were introduced.

### Launcher setup contract

`OVMS_AGENT.COM` now auto-defines user-mode `OVMS_AGENT_GITHUB_BRIDGE` to the shipped checkout `GITHUB_BRIDGE.COM` only when no bridge logical already resolves. An explicitly supplied bridge therefore remains authoritative.

The auto-defined logical is scoped to the launched image and does not persist in the caller after the image exits.

### PR service

The already proven PR service behavior remains intact:

- `pr help`
- `pr list`
- `pr create BASE HEAD ISSUE TITLE`

PR create continues to build its JSON body as native OpenVMS `STREAM_LF` and send it with `--data-binary`, preserving the VAX/RMS transport fix established during M274.

### Issue service

The shipped bridge now supports `operation=issues` with:

- `issues help`
- `issues list [open|closed|all]`
- `issues create TITLE`

The implementation reuses the active saved GitHub profile in `SYS$LOGIN:OVMS_AGENT_GITHUB.DAT`, uses the same protected temporary authorization-header model, and creates POST bodies as `STREAM_LF` files.

### Approval and cleanup

GitHub issue/PR service operations remain gated by FULL approval in OVMS Agent core.

The bridge removes its temporary header, body, and API-response files across all OpenVMS RMS versions. The core service request/response temporary files were also verified absent after live acceptance.

### Documentation

`README.md` now documents:

- shipped bridge auto-wiring and explicit override behavior;
- exact PR and issue command grammar;
- the current one-token title constraint for create commands;
- required GitHub token permissions, including `Pull requests: write` for PR creation and `Issues: write` for issue creation when using fine-grained tokens.

## Canonical VAX acceptance — code head

The exact code head `5cfee3bb35ba595eae7d8795c54ff323059be088` was materialized on OpenVMS/VAX using RMS-aware restore of the four changed files.

Git delta was exactly the intended four files and `git diff --check main..HEAD` was clean.

Canonical build:

```text
Building OVMS Agent Version 2...
All regression tests passed.
Build completed successfully.
M278_BUILD_STATUS = "%X00000001"
```

Focused regression:

```text
M250 guarded GitHub regression test passed.
M278_GITHUB_TEST_STATUS = "%X00000001"
```

Repository hygiene:

```text
core.filemode = false
git status --short = empty
```

## Live acceptance — service surface

### FULL approval floor

Before FULL approval:

```text
AGENT/GITHUB/ISSUES help
GitHub operation refused: FULL approval policy is required.
```

After setting FULL approval, both service help paths succeeded.

PR help:

```text
GitHub operation: pr
Status: success
pr help
pr list
pr create BASE HEAD ISSUE TITLE
```

Issue help:

```text
GitHub operation: issues
Status: success
issues help
issues list [open|closed|all]
issues create TITLE
```

PR list:

```text
GitHub operation: pr
Status: success
GitHub PR list request succeeded.
```

Issue list:

```text
GitHub operation: issues
Status: success
GitHub issue list request succeeded.
```

Issue create:

```text
GitHub operation: issues
Status: success
GitHub issue create request succeeded.
```

Independent GitHub verification found exactly the expected acceptance issue #95 with body `Created by OVMS Agent GitHub service bridge.`; it was then closed completed.

### Temporary artifact cleanup

After the live network run, all of these were verified absent:

```text
OVMS_AGENT_GH_REQUEST.TMP
OVMS_AGENT_GH_RESPONSE.TMP
SYS$LOGIN:OVMS_AGENT_GH_API_HEADERS.TMP
SYS$LOGIN:OVMS_AGENT_GH_API_BODY.TMP
SYS$LOGIN:OVMS_AGENT_GH_API_RESPONSE.TMP
```

### Launcher auto-wire proof

A separate final run explicitly deassigned `OVMS_AGENT_GITHUB_BRIDGE` before launching:

```text
%SHOW-S-NOTRAN, no translation for logical name OVMS_AGENT_GITHUB_BRIDGE
```

Without defining any bridge manually, `@OVMS_AGENT` then successfully executed both:

```text
AGENT/GITHUB/PR help
AGENT/GITHUB/ISSUES help
```

After `EXIT`, the caller still showed:

```text
%SHOW-S-NOTRAN, no translation for logical name OVMS_AGENT_GITHUB_BRIDGE
```

This proves the shipped launcher supplies the repository bridge for the Agent image only when needed and does not overwrite or persist caller configuration.

## Reconciliation status

M278 code functionality is complete and #78 is closed completed.

This file is a documentation-only reconciliation follow-up beyond the separately VAX-accepted code head `5cfee3bb35ba595eae7d8795c54ff323059be088`. The reconciliation head must receive its own canonical OpenVMS/VAX acceptance before promotion.
