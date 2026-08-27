# M277 Reconciliation — Guarded File-Creation Interoperability

## Milestone identity

- Milestone: M277
- Tracking issue: #92
- Development branch: `m277-create-interoperability`
- Base main: `c9aacd6ac7b2942f7bf3cb07c9740395caa6e780` (promoted M276)
- VAX-green code head: `a8f538e8f9a93c7e83691b603b319ca72fa0694e`
- Scope defects: #79, #82
- Out of scope: #78 GitHub bridge setup/docs/service completeness

## Scope and result

M277 addressed two related file-creation interoperability gaps while preserving the existing guarded-write safety model.

1. #79 — Groq Qwen `AGENT/CREATE` Responses interoperability.
2. #82 — persistent workspace continuation had no supported create-file continuation path.

Both defects were closed after canonical VAX build/regression acceptance and live runtime acceptance.

## #79 — Groq Qwen create-response interoperability

The original M274 failure occurred after a Groq/Qwen `AGENT/CREATE` request reached the response parser but produced neither recognized output text nor a recognized function call.

M277 intentionally used a diagnostic-first design instead of guessing a provider-specific alternate schema. `AGENT/CREATE` now has fail-closed telemetry that summarizes sanitized `type` values from the Responses `output` array only after normal output-text and `function_call` parsing both fail. The diagnostic does not reinterpret unknown items and cannot authorize a write.

Deterministic regression coverage verifies response-type summarization, including `reasoning`, `message`, and `output_text`, plus an empty-output case.

Live acceptance with provider `groq`, model `qwen/qwen3.6-27b` produced a standard recognized `create_file` proposal:

```text
Create new file: M277_QWEN_PROBE.TXT
Size: 18 bytes
Proposed contents:
------------------
M277_QWEN_PROBE_OK
------------------
Create file [y/N]? N
File creation cancelled.
Tool requested: create_file M277_QWEN_PROBE.TXT
File creation cancelled. Agent complete.
```

The probe file remained absent and the worktree remained clean.

Important conclusion: M277 did **not** add a speculative Groq/Qwen parser compatibility hack. Current live Groq/Qwen behavior is compatible with the existing normal `function_call` parser. M277 retains the added fail-closed diagnostic so future provider response-shape drift is observable instead of opaque.

## #82 — persistent-session guarded create continuation

The prior persistent continuation path rebuilt session context but routed workspace/full execution through `llm_agent_write()`, which does not expose `create_file`.

M277 adds an explicit command:

```text
AGENT/EXEC/RESUME/CREATE <goal>
```

The implementation:

- rebuilds the active persistent-session context;
- requires workspace or full effective approval policy;
- independently requires `OVMS_AGENT_WRITE_ENABLED`;
- records the new session goal and transcript execution marker;
- delegates to the existing guarded `llm_agent_create()` implementation;
- does not broaden ordinary `AGENT/WRITE` semantics;
- preserves the existing confirmation gate and one-file create behavior.

Live acceptance used persistent session `152C794B` and verified all required gates.

### Read-only policy

The command refused before provider execution:

```text
Persistent file creation requires workspace or full approval policy.
```

### Workspace policy with write support enabled

The resumed create path reached the normal guarded `create_file` confirmation. The proposal was declined and no file was created. The persistent session retained its goal and execution count.

### Workspace policy with write support disabled

After explicitly deassigning `OVMS_AGENT_WRITE_ENABLED`, the command refused before provider contact:

```text
Persistent file creation requires OVMS_AGENT_WRITE_ENABLED.
```

### Cleanup

The M277 test session was deleted with `AGENT/SESSION/DELETE 152C794B`; current session returned to none. `M277_RESUME_PROBE.TXT` remained absent. Final VAX worktree status was clean.

## Code delta at accepted code head

Relative to M276 promoted main, the accepted M277 code head changes exactly these files:

```text
M  SRC/COMMAND_AGENT_M259.C
A  SRC/LLM_CREATE_CONTEXT_M277.INC
M  SRC/LLM_CREATE_CORE.INC
M  SRC/LLM_PARITY_M258.C
M  SRC/m251_create_test.c
```

No build/linker option file change was required.

## Deterministic regression coverage

The existing M251 create-content regression was extended rather than adding another standalone executable. M277 coverage includes:

- empty create content remains rejected;
- Markdown-fence create content remains rejected;
- unrecognized Responses output-type summarization is deterministic and bounded;
- persistent create eligibility requires write support;
- read-only policy is ineligible;
- workspace/full policy is eligible only with write support.

Direct VAX execution produced:

```text
Proposed file content is empty; creation rejected.
Proposed file contains a Markdown code fence; creation rejected.
M251 create-content validation test passed.
```

with `%X00000001`.

## Canonical VAX acceptance

The exact code head `a8f538e8f9a93c7e83691b603b319ca72fa0694e` was materialized on OpenVMS/VAX with RMS-aware restore for all changed files.

Canonical build output:

```text
Building OVMS Agent Version 2...
All regression tests passed.
Build completed successfully.
Run with: $ @OVMS_AGENT
```

Captured status:

```text
M277_BUILD_STATUS = "%X00000001"
M277_CREATE_TEST_STATUS = "%X00000001"
```

Additional final invariants:

- `core.filemode=false`
- tracked worktree clean
- live Groq/Qwen create proposal was confirmation-gated and declined
- persistent create policy/write gates behaved as designed
- test session and runtime artifacts were cleaned up

## Safety and compatibility

M277 preserves the existing safety model:

- unknown provider response shapes remain fail-closed;
- no diagnostic path can write;
- persistent creation still requires workspace/full policy;
- `OVMS_AGENT_WRITE_ENABLED` remains an independent hard gate;
- actual creation still uses the existing confirmation-gated create primitive;
- normal `AGENT/WRITE` behavior is unchanged.

New linker-visible names remain within the DEC C/VAX 31-character identifier constraint, and the accepted build is authoritative for DEC C/VAX compatibility.

## Milestone conclusion

M277 closes #79 and #82 with canonical VAX and live-runtime evidence. The code head `a8f538e8f9a93c7e83691b603b319ca72fa0694e` is VAX-green. This reconciliation document is a documentation-only follow-up and requires its own final VAX acceptance before promotion.
