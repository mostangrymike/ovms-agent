# Changelog

This file records user-visible OVMS Agent milestone changes. Permanent lightweight `mNNN` tags identify canonical promoted milestone commits; public releases additionally use semantic `vX.Y.Z` tags.

The changelog is maintained from M278 forward. Earlier history remains available through Git history, issues, and historical records under `DOC/`.

## Unreleased

No additional user-visible product changes are currently recorded beyond M301 below.

## M301 — Larger native multiline prompts

Tag: `m301`

- Raised native `.END` multiline prompt/goal capacity from the ordinary 8192-byte command buffer to a dedicated bounded 32768-byte (32 KiB) buffer while preserving the 8192-byte one-line limit.
- Kept M251 `.END`, embedded-newline, empty/EOF, and overflow-drain semantics plus M299 trigger selection and `/MULTILINE` compatibility aliases unchanged.
- Added deterministic regression coverage proving preservation of multiline input above 8 KiB and drain/recovery when the new 32 KiB bound is exceeded.
- During acceptance, separately fixed and promoted prerequisite defects #185, #187, and #189 so the original long COBOL `AGENT/PLAN` stress prompt could proceed through RMS-safe Git refresh, large project-instruction composition, and saved-plan scope validation rather than failing downstream after successful multiline capture.
- Final source-green candidate `64595b01f218e12aec32daea5c92749942211c04` (tree `dbe88790861d7cbe53443f41b45ae54600d63a30`) passed canonical OpenVMS V9.2-3 x86-64 full `@BUILD` with `All regression tests passed.`, `Build completed successfully.`, `%X00000001`, and fresh M251/M299/M301 focused regression `%X00000001`.
- The original unshortened COBOL `AGENT/PLAN` prompt then completed normal provider/tool planning and persisted `Implementation plan saved to OVMS_AGENT_PLAN.TXT.` without execution.
- HELP-only final candidate `21ee3f1fd81625010f42e4409d4d552656781ca8` (tree `6077bf3df9124e6189ab50931d460d2a36f9b786`) passed `LIBRARY/HELP/CREATE=(KEYSIZE:64)` with `%X10000001` and `OVMS_AGENT COMMANDS MULTILINE_INPUT` lookup with `%X00000001`.
- Promotion PR #191 produced main `0d09a5575bcf5e05c5e83ff19e10f38675f74e2f`, whose tree exactly equals the final accepted M301 tree. Permanent tag `m301` resolves to that promoted main commit; the merge SHA itself was not separately native-built.
- Issue #192 tracks a separate operating-guide correction after M301 validation proved the then-documented RMS `.git` filespec examples invalid on the canonical host.

## M300 — Blocking-default LLM transport with opt-in streaming

Tag: `m300`

- Made bounded blocking Responses transport the canonical/default provider path instead of attempting SSE streaming by default.
- Added persistent Boolean setting `streaming`, default `OFF`, stored with ordinary settings in `SYS$LOGIN:OVMS_AGENT_SETTINGS.DAT`.
- Added `SETTINGS GET streaming`, `SETTINGS SET streaming ON|OFF`, `SETTINGS TOGGLE streaming`, interactive menu display/change, persistence, and reset/default coverage.
- Added no new logical to enable streaming. The existing `OVMS_AGENT_NOSTREAM` logical remains only as a compatibility/debug force-blocking override during M300.
- When `streaming=ON`, only otherwise eligible interactive non-tool textual responses may use SSE presentation. Requests containing tool definitions always use bounded blocking transport regardless of provider/model; redirected/plain-output operation and completed-response workflows such as `AGENT/PLAN` remain blocking/non-incremental.
- Replaced the normal M294-M296 provider/model stream-exception decision with the generic tool-bearing blocking invariant while preserving the common Responses API and the proven M273 byte-oriented SSE implementation for optional presentation.
- Added deterministic M300 stream-policy regression coverage and extended M280 settings regression coverage for default OFF, persistence, and reset behavior.
- Exact native-green source checkpoint `576ce40727648158ddedf41000ed490788b8b94c` (tree `7d530fad4f09d0d2f288d2e623ca98bb3b7b494e`) passed canonical OpenVMS V9.2-3 x86-64 full `@BUILD` with `BUILD_STATUS = "%X00000001"`, focused M300 transport-policy regression, and focused M280 settings regression.
- Issue #179 remains the separate optional-streaming child-completion/status defect; M300 removes it from default operation rather than falsely declaring the optional SSE path fixed before its live gate.

## M299 — Native `.END` multiline prompt/goal input

Tag: `m299`

- Added native multiline entry for eligible free-form LLM prompt/goal commands without requiring a `/MULTILINE` qualifier.
- Preserved immediate one-line behavior when prompt/goal text is present on the command line.
- Added multiline triggering after required fixed leading arguments for `AGENT/IMAGE image-path`, `AGENT/SESSION/EXEC <id>`, and `AGENT/EXEC/FORK <new-name> ::` when the free-form goal is omitted.
- Preserved M251 reader semantics: `.END` must appear by itself on a physical line, embedded newlines before it are preserved, and existing overflow/EOF handling remains authoritative.
- Kept `AGENT/MULTILINE`, `AGENT/CREATE/MULTILINE`, and `AGENT/REPAIR/MULTILINE` as compatibility aliases.
- Deliberately left structured/safety-sensitive grammars such as DCL, RUN, PATCH, MCP, GitHub operations, and history/query commands outside implicit multiline behavior.
- Added focused trigger-selection regression coverage and synchronized `OVMS_AGENT.HLP`, including the `OVMS_AGENT COMMANDS MULTILINE_INPUT` topic.
- Exact canonical native-accepted candidate `0827cb80ab25e7cf6e5336a5b25396fc8ebd3b81` (tree `a861600d498abf4b910e793e8808c82e878ac4c5`) passed full `@BUILD` with `BUILD_STATUS = "%X00000001"`, focused M251/M299 regression, HELP-library creation/lookup, and live blocking-transport multiline ASK acceptance.
- Promotion PR #180 produced main `18a78a656825f6eb0841c60e1f924fc027980659`, whose tree exactly equals the native-accepted candidate tree; permanent tag `m299` points to that promoted main commit.
- A separate pre-existing streaming completion-status defect exposed during validation is tracked as issue #179 and is not part of the M299 multiline implementation.

## M298 — OpenVMS platform knowledge packs

Tag: `m298`

- Added a separate bounded `KNOWLEDGE/` platform layer for validated OpenVMS Git, RMS, and DCL guidance rather than mixing generic platform lessons into language packs.
- Added strong prompt-topic detection with an 8 KiB bound per platform pack and a 16 KiB combined platform-knowledge budget; relevant Git/RMS/DCL packs may compose while unrelated requests remain unchanged.
- Composed platform guidance with the existing M288 language-pack request-instruction path so language and platform knowledge can both apply when independently relevant.
- Added an explicit guidance-not-evidence boundary: platform packs cannot prove current refs/files/status/system state and cannot override sandbox, safety, evidence, approval, write, network, or tool policy.
- Added provider-free positive, combined-topic, false-positive, injection, unchanged-request, and language/platform-composition regression coverage.
- Added launcher wiring through `OVMS_AGENT_KNOWLEDGE_DIR` while preserving an explicit preexisting override.
- Exact native-green source candidate `d9ecacd80238b157a5b3ebd6f2af62d2f2025785` (tree `9955a9814adcf432d5bedd46d443c43a5bcc060b`) passed full `@BUILD` with `BUILD_STATUS = "%X00000001"`, focused M288/M298 regressions with `%X00000001`, and live blocking Git-positive/unrelated-negative injection checks.
- Final documentation head `a388c918d4f2233c8ce9c858d482aeb6b6498636` passed native HELP-library creation and `PLATFORM_KNOWLEDGE` lookup; PR #176 promoted that accepted tree to main before this docs-only milestone-status synchronization.

## M297 — Reasoning/output-budget exhaustion handling

Tag: `m297`

- Classified incomplete provider responses whose `incomplete_details.reason` is `max_output_tokens` before the raw-response fallback when no assistant text was returned.
- Replaced the raw provider JSON fallback for that case with a bounded diagnostic, optional reported output/reasoning token counts, and retry guidance.
- Added deterministic provider-free regression coverage and synchronized `OVMS_AGENT.HLP` for the user-visible ASK behavior.
- The final accepted documentation tree `4fbab9f2648c0d2182df1845101a15124bb0aa56` was promoted as main `1224014d544b43cea6e9c602ff17217969cb46c6`; the promoted merge SHA carries the accepted tree rather than being separately claimed as the exact native-built source SHA.

## M296 — OpenRouter/free tool transport compatibility

Tag: `m296`

- Revalidated historical OpenRouter/free issue #123 on OpenVMS V9.2-3 x86-64 using the configured `openrouter` profile and exact model `openrouter/free`.
- `PROVIDER TEST openrouter` remained healthy, while the same read-only tool request failed through normal streaming and succeeded through the bounded blocking Responses transport.
- Extended the pre-send transport selector so the OpenRouter endpoint with exact model `openrouter/free` uses bounded blocking transport only when the serialized request contains tool definitions.
- Kept non-tool OpenRouter/free traffic, other OpenRouter models, and unrelated provider/model combinations on the existing streaming decision.
- Added deterministic selector coverage for OpenRouter/free tool and non-tool requests while preserving streaming for a different OpenRouter model.
- Native exact source head `f9c40b6d47d46a21e5b6e2b6b0bbf4f20fd07a2a` passed full `@BUILD`, all regressions, automatic read-only tool execution, and a guarded disposable `AGENT/WRITE` create workflow without defining `OVMS_AGENT_NOSTREAM`.
- The guarded write probe created exactly one RMS version with the requested content; OpenRouter/free subsequently requested an unnecessary `replace_text`, but no second write materialized.
- #124 remains separate reasoning/output-budget work.

## M295 — Groq Qwen tool transport compatibility

Tag: `m295`

- Revalidated historical Groq/Qwen issues #120 and #125 on OpenVMS V9.2-3 x86-64 using `qwen/qwen3.6-27b`.
- Current streaming tool requests failed before a usable tool exchange, while the same read-only tool workflow succeeded through the bounded blocking Responses transport.
- Extended the existing pre-send Groq tool transport selector so exact model `qwen/qwen3.6-27b` uses bounded blocking transport when the serialized request contains tool definitions.
- Kept non-tool Qwen traffic, other Qwen models, and non-Groq Qwen endpoints on the existing streaming decision.
- Added deterministic selector coverage for the exact Qwen model, non-tool requests, another Qwen model, and a non-Groq endpoint.
- Native exact source head `909a3c86b6028c16f1c489d3e2def208845449a1` passed full `@BUILD`, all regressions, automatic read-only tool execution, and a guarded disposable `AGENT/WRITE` create workflow without defining `OVMS_AGENT_NOSTREAM`.
- The guarded write probe created exactly one RMS version with the requested content; Qwen subsequently requested an unnecessary `replace_text`, but no second write materialized.
- #124 remains separate reasoning/output-budget work; #123 remains separate OpenRouter/free compatibility work.

## M294 — Groq GPT-OSS tool transport compatibility

Tag: `m294`

- Removed the historical pre-rejection that prevented Groq `openai/gpt-oss-*` models from attempting Responses tool workflows.
- Added deterministic eligibility coverage for GPT-OSS tool requests.
- Revalidated live provider behavior after M290 bounded transport hardening: plain provider health is responsive, streaming tool requests fail quickly, and the same tool workflows succeed through the bounded blocking Responses transport.
- Added a narrow pre-send transport selector: Groq `openai/gpt-oss-*` requests that actually contain tool definitions use the bounded blocking transport automatically; non-tool GPT-OSS requests and other provider/model combinations keep normal streaming behavior.
- The selector is not a failed-stream retry and does not duplicate an already-sent provider request.
- Native OpenVMS V9.2-3 x86-64 validation on exact source head `d7e62df01aac2b0a4d8402fb12f95b1044ba0358` passed full `@BUILD`, automatic read-only `AGENT` tool execution, and a guarded disposable `AGENT/WRITE` create workflow without requiring `OVMS_AGENT_NOSTREAM`.
- The guarded write probe also showed one unnecessary model-requested `replace_text` after successful creation, but no second RMS write materialized; the file remained only version `;1` with exact requested content.

## M293 — Markdown documentation currentness

Tag: `m293`

- Audited every repository `*.MD` file against promoted M292 product state.
- Updated living root documentation to identify OpenVMS V9.2-3 x86-64 as the current canonical native acceptance host while preserving historical DEC C/VAX compatibility constraints where they remain part of the source contract.
- Synchronized current documentation for M289 native toolchain profiles/`build_source`, M291 atomic ordinary `AGENT/WRITE` and Git/RMS/auth hardening, and M292 provider response-health diagnostics.
- Reframed frozen parity material so M252-M260/Codex-parity documents are not mistaken for current product command documentation.
- Preserved historical reconciliation/audit facts and language-pack evidence rather than rewriting the platform terminology of old milestones.
- No source, build procedure, launcher, HELP command surface, SETTINGS behavior, or runtime behavior changes.

## M292 — Provider health test diagnostics

Tag: `m292`

- Added `PROVIDER TEST` to test all configured provider profiles and `PROVIDER TEST name` to test one profile without changing the active provider.
- Sends one bounded real plain-text no-op request through each profile's configured transport and reports model, elapsed time, and textual state.
- Responsive profiles report `OK`; failures distinguish `CONFIG`, `LOCAL ERROR`, `TRANSPORT`, `API ERROR`, `EMPTY`, and `INVALID`.
- Added green/red ANSI presentation while retaining textual states for plain terminals/logs.
- Uses dedicated temporary request/header/response files, removes all RMS versions after the probe, and never prints credentials.
- Added provider-free deterministic response-classification regressions and synchronized `OVMS_AGENT.HLP`.
- Native-accepted exact candidate `a732840d4f5195b950dbcdb67b14bebe24af64b7` on OpenVMS V9.2-3 x86-64; promoted main `c6eda68f573e407d405a539874f518853b50f86c` carries the identical tree `76b9ffe15dd3b52cf3efb08c684fee01a2327db6`.

## M291 — Remaining reliability backlog hardening

Tag: `m291`

- Resolved the remaining reliability/workflow backlog tracked by #154 while keeping provider/model compatibility as separate work.
- Added local confirmation for model-invoked `structured_patch` in guarded write workflows.
- Added conservative COBOL one-for-one paragraph-rename protection.
- Cleared transient ANSI turn status before durable request-construction diagnostics.
- Hardened `GITAUTH --check` into a non-mutating current-origin write-authorization preflight; setup retains a config-only check.
- Filtered OVMS Agent's own exact Git scratch basenames from synthesized untracked status without hiding tracked same-name paths/lookalikes.
- Made ordinary `AGENT/WRITE` existing-file editing atomic through one `structured_patch`; sequential replace primitives remain supervised `AGENT/FIX` behavior.
- Reverified the historical Git-discovery report and closed it `not_planned` after a fresh valid OpenVMS worktree proved current discovery works and the historical metadata was structurally incomplete.
- Native-accepted exact candidate `b94aee71eeec0d991273f60012bbf491f934d5e4`; promoted main `82fd65c9c6b75eead02387d7ed1812ca3280bc4e` carries the identical accepted tree.

## M290 — Reliability hardening and backlog reduction

Tag: `m290`

- Hardened RMS structured-patch integrity.
- Corrected subprocess/Git status propagation so encoded POSIX nonzero exits are not mistaken for native odd-success statuses.
- Fixed Git/RMS direct-read short-read behavior.
- Isolated persistent-settings regressions from user settings.
- Added bounded provider transport timeout behavior.
- Completed the remaining reliability-backlog audit scoped by #152.
- Native-accepted exact development head `8e1c96bfcf02d1d8114644ea0a2ee34a5b520bb9`; promoted main `587574a7d68e09a86f8975d6b889ea4ccf45675f` carries the identical accepted tree.

## M289 — Native toolchain profiles and guarded source build execution

Tag: `m289`

- Added bounded `LANGUAGE/<LANGUAGE>.BUILD` native toolchain profiles selected by source extension.
- Added guarded `build_source` execution requiring full approval, guarded writes, and DCL execution.
- Added compiled profiles for MACRO32, Fortran, COBOL, Pascal, C++, and BASIC with native compile/link sequencing and failure propagation.
- Added compile-only Java support, interpreted Python/Perl support, and validated project-relative DCL procedure execution.
- Added focused parser/executor regressions and synchronized tool-schema/HELP documentation.
- Native-accepted every shipped M289 language class on OpenVMS V9.2-3 x86-64; final product/HELP acceptance head was `b114b422cf41bd4eb1409ec47593c837c4406418`.

## M288 — Shared OpenVMS language knowledge packs

Tag: `m288`

- Added data-driven `LANGUAGE/<LANGUAGE>.MD` knowledge packs that can be injected into AI requests when strong language evidence is present.
- Added a 16 KiB pack bound and an explicit policy boundary preventing language guidance from overriding sandbox, safety, evidence, approval, or write rules.
- Added native-validated COBOL and Fortran packs derived from multi-language campaign evidence.
- Added provider-free detection/injection regressions and launcher support through `OVMS_AGENT_LANGUAGE_DIR`.
- Kept unrelated requests unchanged when no confident language/matching pack exists.

## M286 — Uppercase root documentation and Markdown README

Tag: `m286`

- Normalized the remaining root documentation filenames to uppercase `.MD` names.
- Reworked `README.MD` into conventional Markdown and updated live root-document cross-references.
- Preserved historical `DOC` records instead of rewriting them solely for filename-case modernization.
- No product command, SETTINGS, build, or runtime behavior changes.

## M285 — Canonical uppercase DOC history directory

Tag: `m285`

- Consolidated duplicate-case documentation history so uppercase `DOC` is the sole canonical documentation-history directory.
- Moved the Codex parity specification and M252-M260 parity audits into uppercase `DOC` while preserving their historical contents.
- Removed the obsolete lowercase `doc` tree.
- No product/runtime behavior changes.

## 1.0.0 — 2026-08-28 (M284)

Tags: `m284`, `v1.0.0`

- Declared the first stable semantic release of OVMS Agent.
- Set the runtime and `VERSION` command semantic version to `1.0.0`.
- Added the MIT License.
- Aligned installation/build/README/changelog/release guidance with the 1.0.0 contract.
- Added release acceptance guidance requiring the then-canonical OpenVMS/VAX build, regressions, successful OpenVMS status, and startup/`VERSION` smoke test from the exact candidate.
- Performed a release-blocker audit for stale version strings, TODO/FIXME markers, and obvious committed credential material.
- M284 was release hardening rather than a new command/SETTINGS feature milestone.

## M283 — Root project documentation set

Tag: `m283`

- Added the root installation, build, contributor, security, architecture, and changelog documents.
- Added a documentation index to README and aligned its SETTINGS summary.
- Established ownership between README overview, HELP command/settings reference, build/install guidance, contributor workflow, architecture, security, and changelog.

## M282 — OpenVMS HELP reference

Tag: `m282`

- Added root-level `OVMS_AGENT.HLP` as hierarchical OpenVMS HELP source.
- Documented the then-current user-facing commands and persistent SETTINGS surface with practical when/why guidance.
- Established the durable rule that command/SETTINGS changes update HELP in the same milestone.
- Validated HELP-library creation and representative topic hierarchy on the canonical OpenVMS system used for M282.

## M281 — Persistent autonomous limits in SETTINGS

Tag: `m281`

- Added persistent `auto_turns` and `auto_writes` SETTINGS with interactive entries and noninteractive GET/SET support.
- Preserved higher-precedence legacy logical overrides.
- Established defaults of 12 turns / 3 writes and supported ranges of 1-32 / 1-8.

## M280 — Interactive SETTINGS

Tag: `m280`

- Added persistent interactive SETTINGS for guarded writes, DCL execution, provider/model, output-token limit, approval policy, network policy, and related noninteractive forms.
- Preserved legacy logical-name compatibility and kept credentials separate from ordinary settings.

## M279 — Legacy backlog closure

Tag: `m279`

- Closed the remaining legacy defect backlog carried after M278.
- Reconciled structured symbol/refactoring and agent planning/safety work outside the earlier baseline.
- Preserved separate create-action ceiling behavior later used by autonomous SETTINGS compatibility.

## M278 — GitHub service bridge completion

Tag: `m278`

- Completed the shipped GitHub service bridge for pull-request and issue operations.
- Added launcher auto-wiring while preserving explicit overrides.
- Reused the active saved GitHub profile rather than adding a second credential store.
- Used protected short-lived authorization/body/response files with RMS-version cleanup.
- Preserved FULL approval for GitHub service mutations.
- Verified live PR/issue service behavior on the OpenVMS acceptance environment used at that milestone.

## Changelog policy

For future milestones:

- record in-progress user-visible work under `Unreleased` or the pending milestone section;
- on promotion, identify the permanent milestone tag and semantic release tag when applicable;
- describe behavior/compatibility/security/build/documentation changes that matter to users/contributors rather than every internal commit;
- preserve historical platform/evidence facts rather than retroactively rewriting old acceptance records; and
- distinguish an exact native-tested candidate SHA from a later GitHub merge SHA that merely carries an identical accepted tree.