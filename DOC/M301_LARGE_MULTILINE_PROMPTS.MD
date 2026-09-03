# M301 Large Native Multiline Prompts

M301 raises the capacity of native `.END`-terminated free-form prompt/goal input without changing the ordinary one-line command buffer or M299 trigger grammar.

## Behavior

- Ordinary one-line command input remains bounded by `OVMS_AGENT_INPUT_SIZE` at 8192 bytes.
- Native multiline collection uses a dedicated on-demand `OVMS_AGENT_MULTILINE_SIZE` buffer of 32768 bytes (32 KiB), including the terminating NUL.
- The M251 multiline reader remains authoritative for exact `.END` termination, embedded-newline preservation, empty-body handling, EOF handling, and overflow draining.
- If the 32 KiB multiline bound is exceeded, OVMS Agent drains input through `.END`, reports `Multiline prompt is too long.`, and leaves the following command readable.
- M299 trigger selection is unchanged: eligible free-form commands enter multiline mode only when their free-form tail is omitted after any required fixed leading operands.
- `/MULTILINE` forms remain compatibility aliases rather than requirements.

## Implementation boundary

`SRC/AGENT.H` defines the independent one-line and multiline capacities. `SRC/MAIN.C` allocates the larger multiline buffer only when native multiline mode is selected, executes the collected command from that buffer, and frees it afterward. `SRC/m251_multiline_test.c` covers both preservation above 8 KiB and recovery after overflow beyond the 32 KiB bound.

## Prerequisite fixes discovered during acceptance

The original long COBOL `AGENT/PLAN` stress prompt exposed two downstream defects that were intentionally fixed and promoted separately before final M301 acceptance:

- #187 / PR #188 enlarged bounded agent-context composition so a successfully captured large prompt was not rejected later by fixed instruction/context scratch buffers.
- #189 / PR #190 made fallback saved-plan scope parsing line-oriented so explanatory prose about possible future paths was not misclassified as explicit modify/create scope.

A nested RMS-safe Git refresh defect discovered during the campaign was likewise fixed separately as #185 / PR #186.

## Canonical native acceptance

Canonical host: OpenVMS V9.2-3 x86-64.

Final source-green candidate:

- SHA `64595b01f218e12aec32daea5c92749942211c04`
- tree `dbe88790861d7cbe53443f41b45ae54600d63a30`

Source/regression evidence:

- exact fetched/ref identity verified;
- full `@BUILD` reported `All regression tests passed.` and `Build completed successfully.` with `%X00000001`;
- fresh `M251_MULTILINE_TEST` passed M251.5b, M299 trigger selection, and `M301 large multiline capacity regression passed.` with `%X00000001`;
- the original unshortened COBOL `AGENT/PLAN` prompt was accepted through `.END`, passed project-instruction composition and saved-plan scope validation, completed provider/tool planning, and persisted `Implementation plan saved to OVMS_AGENT_PLAN.TXT.` without executing the plan.

Final HELP-only candidate:

- SHA `21ee3f1fd81625010f42e4409d4d552656781ca8`
- tree `6077bf3df9124e6189ab50931d460d2a36f9b786`

HELP evidence:

- RMS-aware restore verified the effective `OVMS_AGENT.HLP` version;
- `LIBRARY/HELP/CREATE=(KEYSIZE:64)` returned `%X10000001`;
- `OVMS_AGENT COMMANDS MULTILINE_INPUT` lookup displayed the 32768-byte multiline bound, unchanged 8192-byte one-line bound, overflow-drain behavior, eligible commands, and compatibility aliases;
- lookup returned `%X00000001`.

Promotion PR #191 merged as main `0d09a5575bcf5e05c5e83ff19e10f38675f74e2f`. Its tree is exactly `6077bf3df9124e6189ab50931d460d2a36f9b786`, equal to the final accepted M301 candidate tree; the merge SHA itself was not separately native-built. Permanent lightweight tag `m301` resolves to that promoted main commit.

## Follow-up discovered during HELP validation

Issue #192 tracks a separate operating-guide defect: the then-documented RMS `.git` filespec examples such as `[.^.GIT]FETCH_HEAD.` were rejected with `%DCL-W-DIRECT` on the canonical host. M301 acceptance did not depend on those invalid examples; exact Git ref identity, RMS-aware file restore, native build/regressions, live behavior, and HELP validation were independently established.
