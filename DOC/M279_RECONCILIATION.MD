# M279 Reconciliation — Legacy Defect Backlog Closure

## Scope

M279 audits and closes the remaining legacy defect backlog carried forward after M278, including deterministic/client-side fixes, provider-facing compatibility, refactor verification, and one additional RMS/version-safety defect exposed during live VAX acceptance.

Base canonical main before M279:
- `dfd552f3cda772fd07c10600a6aba9d9edad0cbc` (promoted M278)

Latest VAX-green M279 code head:
- `7200ab5492b9f22c25ef547b15b4058110ad47b1`

Tracking issues:
- #97 `M279 legacy defect backlog closure`
- #98 `M279 subtask: deterministic backlog hardening tranche`
- #99 `M279 provider normalization tranche`
- #100 `M279 acceptance ledger`

## Closed defect inventory

Already-fixed stale defects closed at M279 audit start:
- #52
- #61
- #62
- #63
- #64
- #66

Defects implemented and accepted during M279:
- #53 failed AGENT/BUILD output handoff to AGENT/REPAIR/PLAN
- #54 AGENT/WRITE multi-file scaffolding through create_file
- #55 flat-root local filenames in legacy plan scope
- #56 provider-neutral bounded output token cap
- #59 completed empty Responses payload classification
- #60 structured patch session-approval override
- #65 SHOW/STRUCT local-declaration misclassification
- #67 Groq GPT-OSS Responses-tool transport guard
- #70 OpenRouter assistant-text tool-call fail-closed behavior
- #71 incidental NULL goal-guard handling
- #73 flat-root refactor object invalidation
- #76 sibling tool-call continuation filtering and large guarded-edit reads
- #101 RMS/version-safe RENAME/APPLY and RENAME/VERIFY transaction/rollback

The full live GitHub open-issue audit after #101 acceptance found only M279 tracking issues #97 and #100 still open. #74 was explicitly rechecked and is already closed completed. Accidental connector placeholder issues #102 and #103 were immediately closed `not planned` and are not product work items or part of M279 scope.

## Net code changes

Relative to promoted M278 main, the VAX-green M279 code head changes 28 files, all under `SRC/`:

- `SRC/LLM_AGENT_CTX.C`
- `SRC/LLM_AGENT_LOOP_M259_CORE.C`
- `SRC/LLM_AUTO.C`
- `SRC/LLM_AUTO.H`
- `SRC/LLM_AUTO_CREATE_M279.INC`
- `SRC/LLM_BUILD_CORE.INC`
- `SRC/LLM_GOAL_GUARD.INC`
- `SRC/LLM_PATCH.C`
- `SRC/LLM_PLAN_SCOPE_M279.INC`
- `SRC/LLM_PLAN_SENSITIVE.INC`
- `SRC/LLM_PROMPTS.C`
- `SRC/LLM_PROMPTS.H`
- `SRC/LLM_REQUEST_AGENT.C`
- `SRC/LLM_REQUEST_AGENT.H`
- `SRC/LLM_REQUEST_BASIC.C`
- `SRC/LLM_REQUEST_BUILD.C`
- `SRC/LLM_REQUEST_IMAGE_M259.C`
- `SRC/LLM_REQUEST_LIMIT.INC`
- `SRC/LLM_RESPONSE.C`
- `SRC/LLM_RESPONSE.H`
- `SRC/LLM_TOOLS_M264.C`
- `SRC/LLM_TOOL_SCHEMA.C`
- `SRC/SYMBOL_INDEX.H`
- `SRC/SYMBOL_RENAME_M279.INC`
- `SRC/SYMBOL_SHOW_M279.INC`
- `SRC/SYMBOL_SYSTEM_M279.INC`
- `SRC/m251_repair_goal_test.c`
- `SRC/m251_synth_test.c`

The large legacy `SRC/SYMBOL_INDEX.C` translation unit remains untouched.

## Canonical VAX acceptance — code head

The exact code head `7200ab5492b9f22c25ef547b15b4058110ad47b1` was materialized on OpenVMS/VAX with RMS-aware restore.

Canonical build:

```text
Building OVMS Agent Version 2...
All regression tests passed.
Build completed successfully.
M279_BUILD_STATUS = "%X00000001"
```

Repository hygiene at the accepted code head:

```text
git diff --name-only = empty
core.filemode = false
```

Earlier focused M279 regressions on accepted ancestor `cad09a3208b287916357db2e092ef56bb391298b` also returned `%X00000001`, covering request serialization, empty-completed response handling, sibling tool-call filtering, tool-like assistant text, GPT-OSS transport guard, large guarded-edit reads, flat-root plan scope, incidental NULL handling, and create validation.

## Live acceptance highlights

### Build-to-repair handoff (#53)

A controlled failing DEC C build produced `%CC-E-UNDECLARED` for `M279_MISSING`. Immediate `AGENT/REPAIR/PLAN` consumed the persisted diagnostic, printed the first actionable diagnostic and source context, and saved the expected one-operation repair plan.

### Multi-file scaffolding (#54)

One ordinary `AGENT/WRITE` transaction created `HELLO.C` and `BUILD.COM` through `create_file`. The final conservative build procedure used `CC HELLO` and `LINK HELLO`; the build returned odd success `%X10000001` and the executable printed `M279_CREATE_OK` with `%X00000001`.

### Session approval (#60)

With session approval set to workspace, structured patch dry-run, validation, and apply succeeded without requiring a separate duplicate approval environment policy. The disposable target changed from `M279_OLD` to `M279_NEW`.

### SHOW/STRUCT (#65)

A disposable flat-root fixture containing both `struct rule_ctx { ... };` and local declaration `struct rule_ctx ctx;` reported the real definition in `types.h`. `REINDEX` found 1 source file / 1 entry, confirming flat-root indexing remained healthy.

### Provider compatibility (#67, #70)

Groq `openai/gpt-oss-120b` agent/tool use failed locally with an explicit incompatible Responses-transport diagnostic before malformed provider/Harmony tool handling.

OpenRouter positive controls using `cohere/north-mini-code:free` and `openrouter/free` both produced genuine structured `read_file_range` calls. Deterministic regression coverage proves JSON-like assistant tool text is diagnosed and never executed.

### Flat-root refactor verification (#73)

On a disposable flat-root C project, `RENAME/VERIFY helper renamed_helper`:
- created a new OpenVMS source version;
- rebuilt the symbol index;
- explicitly deleted root-level `MAIN.OBJ;1` with no `[.BUILD]` search failure;
- ran the controlled build successfully;
- reported `Rename completed successfully.`

Post-verification, `MAIN.C;1` and `MAIN.C;2` both existed, `renamed_helper` appeared in definition and caller, final `@BUILD` returned odd success `%X10000001`, and runtime printed `M279_REFACTOR_7` with `%X00000001`.

### RMS/version-safe rename rollback (#101)

A second disposable test deliberately made `BUILD.COM` fail only while `renamed_helper` was present. The renamed transaction failed verification as intended, then automatically removed only the transaction-created source version, restored the prior version by version selection, reindexed, and rebuilt successfully.

Final evidence:
- `DIRECTORY MAIN.C;*` showed only `MAIN.C;1`;
- `SEARCH MAIN.C "renamed_helper"` returned no matches (`%X08D78053`);
- `SEARCH MAIN.C "helper"` returned `%X00000001` and found both original occurrences;
- final `@BUILD` returned `%X10000001`;
- final runtime printed `M279_ROLLBACK_7` and returned `%X00000001`;
- no blind `main.c;-1` rollback assumption and no incomplete rollback occurred.

## Reconciliation status

All M279 product defects and newly exposed #101 are closed on accepted VAX evidence. Deterministic tracking #98 and provider tracking #99 are closed completed. The live repository issue audit found no additional open product defect outside the M279 tracking issues.

This file is a documentation-only reconciliation follow-up beyond the separately VAX-accepted code head `7200ab5492b9f22c25ef547b15b4058110ad47b1`. The reconciliation head containing this file must receive its own canonical OpenVMS/VAX acceptance before promotion to `main`.
