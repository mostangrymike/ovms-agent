# M276 Reconciliation — Agent-Loop Correctness Hardening

## Scope

M276 hardens autonomous loop completion and retry semantics for backlog issues #58, #68, and #81.

- Base promoted `main`: `24fee6dc9a778c8ec9bf855cc8537717882e9312`.
- Development branch: `m276-agent-loop-hardening`.
- Final VAX-green code checkpoint: `2cf921fe2296b5768961b9331b125859556b30ce`.
- Tracking issue: #90.

This document is reconciliation-only. Its commit does not supersede the code checkpoint above as VAX-tested M276 state unless the reconciliation head itself is separately accepted on VAX.

## #58 — retain one approved guarded write

Original failure: `AGENT/WRITE` could apply a correct approved patch, continue proposing additional writes, then roll the first patch back when the run later exhausted its turn budget.

M276 moves the one-bounded-write rule into controller state rather than relying on model self-restraint:

- once a guarded `AGENT/WRITE` RMS write exists, a later write action is blocked by `llm_auto_allow_write()`;
- an actual provider/tool error after an applied write still rolls the transaction back;
- a turn-limit reached after an extra write was blocked may finish as `bounded-write` and retain the earlier approved change;
- deterministic M230 coverage proves both bounded-write retention and error rollback.

Live production acceptance at `2cf921fe...` used only tracked fixture `TEST/GAP008_RMS.TXT`:

1. Qwen read the two-line fixture.
2. User approved a first `replace_text` changing `repository version` to `repository version M276-FIRST`.
3. Qwen deliberately attempted a second separate `replace_text` for the other line.
4. The controller blocked the second write with `status: limit` / `Autonomous write limit reached`; no second local approval was requested.
5. The run completed with `Write actions: 1`, and the file contained only the first change.
6. `@GIT_RESTORE "TEST/GAP008_RMS.TXT"` returned `%X00000001`, restored exact HEAD content, and left the worktree clean.

Issue #58 was closed as completed.

## #68 — bounded provider rate-limit retry

Original failure: a transient Groq 429-style response containing an explicit retry delay was treated as a successful curl transport and then surfaced as provider error JSON, aborting the agent loop.

M276 adds a narrow bounded retry path:

- recognize only provider error JSON carrying `rate_limit_exceeded` / `Rate limit reached` and an explicit `Please try again in <seconds>s` delay;
- parse decimal delays and round fractional seconds upward;
- reject missing/invalid delays and waits above 60 seconds;
- retry the same request with the existing request/context files;
- allow at most two retries per provider request;
- do not treat authentication/validation failures or ordinary assistant text as retryable;
- surface the wait and retry count to the user.

The parser is compiled into the existing `LLM_TRANSPORT.OBJ` translation unit by `LLM_TRANSPORT_M273.C`; no linker option or build-procedure changes were required. M230 contains deterministic parser coverage.

Live production acceptance occurred naturally during the #58 run. Groq rate-limited two different provider turns. OVMS Agent printed:

- `Provider rate limit; retrying the same request in 3 seconds (1 of 2).`
- `Provider rate limit; retrying the same request in 19 seconds (1 of 2).`

Both retries recovered and the autonomous run continued to a successful final rather than aborting. Existing write/tool state survived the transient failures. Issue #68 was closed as completed.

## #81 — reject evidence-free early finals

Original failure: Groq/Qwen could return intent-only prose such as `I'll inspect the existing source files...` before executing any project tool; OVMS Agent recorded that zero-evidence turn as normal `final` completion.

M276 adds a final-evidence gate for AGENT/WRITE/FIX workflows:

- PLAN remains exempt because a plan may legitimately be returned directly;
- before any project tool has executed, assistant output text is not accepted as a successful final;
- a response containing a real function call still proceeds through normal tool execution;
- zero-tool text is shown to the user together with an explicit evidence-free-early-final diagnostic, and the loop exits through the normal non-final/error path;
- after tool evidence exists, normal final text remains accepted.

M230 deterministically covers zero-tool rejection, post-tool allowance, and the PLAN exemption.

Live production acceptance at `2cf921fe...` used:

`AGENT "Do not execute any project tool. Reply exactly EVIDENCE_FREE_M276_TEST."`

Observed result:

- model text: `EVIDENCE_FREE_M276_TEST`;
- explicit diagnostic: `Agent returned final text before executing any project tool; treating this as an evidence-free early final.`;
- autonomous status: `Turns: 1`, `Tool calls: 0`, `Stop reason: error`;
- process status `%X00000001`;
- exact HEAD unchanged and worktree clean.

Issue #81 was closed as completed.

## Canonical VAX acceptance

The exact code head `2cf921fe2296b5768961b9331b125859556b30ce` was materialized on OpenVMS/VAX with RMS-aware restore for all changed files.

`git diff --name-status main..HEAD` contained exactly:

- `SRC/LLM_AUTO.H`
- `SRC/LLM_AUTO_CORE.INC`
- `SRC/LLM_RATE_LIMIT.C`
- `SRC/LLM_RATE_LIMIT.H`
- `SRC/LLM_RESPONSE.C`
- `SRC/LLM_TRANSPORT.C`
- `SRC/LLM_TRANSPORT_M273.C`
- `SRC/m230_auto_test.c`

Canonical root `@BUILD` printed:

- `All regression tests passed.`
- `Build completed successfully.`

Final build status was `%X00000001`; `git diff --check main..HEAD` was clean and final `git status --short` was empty. As observed in M275, successful quiet-build console strings were not retained in `OVMS_BUILD.LOG`; later `SEARCH` NOMATCHES did not invalidate the direct terminal acceptance evidence.

## Remaining backlog

M276 intentionally does not address the remaining separate capability/provider issues:

- #78 — GitHub bridge setup/docs/service completeness;
- #79 — Groq/Qwen `AGENT/CREATE` response parsing;
- #82 — resumed workspace continuation cannot create files.

Those remain open for later milestones.

## Promotion readiness

After this reconciliation-only commit is separately VAX-accepted:

1. promote `m276-agent-loop-hardening` to `main` through a promotion PR;
2. verify the promoted main tree matches the accepted reconciliation tree;
3. create permanent lightweight tag `m276` on the canonical promoted main commit;
4. close tracking issue #90 with promotion/tag evidence;
5. update `CHAT_SESSION_STATE.json`;
6. apply the milestone branch-retention rule, retaining the two most recent completed milestone development branches only after tags and unique-work checks are verified.
