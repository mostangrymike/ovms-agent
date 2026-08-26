# M274 OVMS Agent Self-Hosting Stress-Test Reconciliation

## Scope

M274 was deliberately different from a normal feature milestone. Its primary deliverable was not the small network-configuration inventory application under `m274_netcfg`; the application was the workload used to force OVMS Agent through real development pressure on OpenVMS. Issue #49 required genuine generation, build/repair, architecture analysis, refactoring, GitHub publication, provider switching, persistent-session continuation across sittings, and a final reconciliation of what the agent actually did and where it failed.

The milestone started from the M273 VAX-green `main` checkpoint `f57b1ebc7ea54d3b9245ae581dcd27290dd52d54`. The final pre-reconciliation M274 parent code checkpoint is `ba31fdbb91487243892472a0d836a6447fe79f71` on `m274-agent-stress-project`. That exact merge head was accepted on VAX after the Phase 6 reporting/query merge: the target build succeeded, `summary` and `query` smoke tests succeeded, the canonical root build printed `All regression tests passed.`, the final status was `%X00000001`, and no tracked working-tree changes remained.

This document is intentionally a reconciliation-only change. Any commit that adds or edits this file does not supersede `ba31fdbb91487243892472a0d836a6447fe79f71` as the VAX-tested M274 code checkpoint unless that later commit is separately accepted on VAX.

## Stress target that was actually built

The target became a real flat-root DEC C/VAX project rather than a synthetic command fixture. Its tracked implementation consists of `build.com`, `main.c`, `storage.c/.h`, `fileutil.c/.h`, `diff.c/.h`, and `rules.c/.h`.

The final CLI includes the original inventory operations plus the Phase 6 reporting layer:

- `add` ingests a local configuration into the persistent inventory store;
- `list` enumerates stored device metadata;
- `show` displays one device in detail;
- `diff` compares a candidate configuration with the stored configuration;
- `check` runs the rule engine and reports warning/error counts plus the extracted risk score;
- `summary` reports device count, total stored configuration bytes, and newest update time;
- `query FIELD VALUE` performs case-sensitive substring matching across `device`, `address`, `path`, or `all` metadata fields and reports `Matches: N`.

The accepted Phase 6 VAX fixture contained one `router1` device at `192.0.2.1`, 83 configuration bytes, and update time `1787620123`. `summary` returned those values; device/address/path/all queries each returned one match; a nonexistent-device query returned `Matches: 0` with success; existing `list`, `show`, `diff`, and `check` behavior remained green.

The target was intentionally kept simple enough that friction would primarily expose OVMS Agent behavior rather than application-domain complexity. That worked: most significant M274 findings were agent, provider, OpenVMS-path, transaction, or regression-coverage defects rather than target design failures.

## OVMS Agent surfaces genuinely exercised

M274 exercised a broad set of shipped behavior in real workflows rather than isolated command demonstrations.

Generation and guarded development paths included `AGENT/WRITE`, `AGENT/CREATE`, `AGENT/BUILD`, `AGENT/REPAIR/PLAN`, `AGENT/FIX`, `AGENT/PLAN`, `AGENT/APPROVAL/SET`, `AGENT/AUTO/LIMITS`, ordinary read/search tools, `GITDIFF`, and deterministic `AGENT/TOOL/RUN` operations including `PATCH` and later `GITRESTORE`.

Architecture and symbol/refactor paths included `REINDEX`, symbol/module queries, `MODULE/GRAPH`, `CLUSTER`, `CYCLE`, `SHOW/STRUCT`, and the required real refactor sequence `EXTRACT/FUNCTION -> RENAME -> MOVE/FUNCTION`.

GitHub paths included a real guarded `AGENT/GITHUB/PUSH` publication during the target build-out and a real `AGENT/GITHUB/PR create` in Phase 4. The latter created PR #80 through the OpenVMS-side GitHub bridge rather than by substituting an external GitHub write for the required agent command.

Provider paths included persisted provider profiles, `PROVIDER` switching, `MODEL` changes, ordinary `ASK`, and agentic tool workflows through Requesty/OpenAI-compatible routing, OpenRouter models, and Groq. The point was not to benchmark model intelligence; it was to test how OVMS Agent normalized materially different provider behaviors.

Persistent-session paths included a base session, `AGENT/SESSION/FORK`, list/resume/current/history/results inspection, `AGENT/EXEC/RESUME`, approval reset/reapproval, and real resumed-session writes. The Phase 6 child `6F535C3D`, forked from `42BABDB9`, survived multiple OVMS Agent process exits/relaunches and more than one development sitting.

These commands were not merely invoked. Their results affected the project: files were created, failed builds were encountered, architecture was queried, a real function was extracted/renamed/moved, GitHub PRs were created, provider incompatibilities changed the implementation route, and a persisted session produced code that was compiled and executed on VAX.

## Surfaces deliberately not manufactured

M274 did not create artificial failures merely to claim command coverage.

`CYCLE` found no dependency cycle after the real graph was indexed, so no synthetic cycle was introduced just to exercise cycle repair. The accepted project graph was acyclic. Similarly, refactor operations beyond those required by the genuine risk-score redesign were not forced into the target without a design reason.

The Phase 4 PR was merged through the normal review/control-plane path; M274 did not require an agent-side merge command after proving that `AGENT/GITHUB/PR create` worked end to end.

Paid provider usage was not expanded simply to obtain more test samples. Once provider-specific evidence was sufficient—and Requesty/OpenRouter account constraints were reached—the milestone stopped model hopping rather than spending money to manufacture additional differences.

The DCL execution gate was not enabled merely for breadth. Where DCL/VAX execution was required for acceptance, the user remained the execution authority.

## Phase 1 — an empty nested project immediately exposed self-hosting assumptions

The first meaningful result occurred before the target existed. Running OVMS Agent from `SYS$SYSDEVICE:[MIKE.OVMS_AGENT.M274_NETCFG]` exposed #50: raw Git correctly resolved the containing repository, but the agent's Git-context capture wrote its temporary status file relative to the repository top level while the wrapper tried to read it relative to the nested project directory. `AGENT/WRITE` therefore failed with `Unable to compose Git context.`

This was exactly the kind of failure M274 was intended to find: the feature had regressions around Git context, but those tests had not represented a real project root inside a larger repository on OpenVMS. #50 was repaired through the self-hosting flow and closed with nested-directory regression coverage before target generation continued.

Repairing #50 then exposed #51. A successful guarded change made the Git diff large enough that the composed context exceeded the request-builder's smaller 8192-byte goal limit even though the upstream composer allowed 32768 bytes. `AGENT/WRITE` stopped before the provider call with `Unable to write complete agent request.` The mismatch was repaired and #51 was closed.

Other Phase 1 observations were less flattering but equally useful. A provider run could return introductory/final prose without performing the requested write. Approval policy had to be elevated from read-only before structured writes were permitted. When an agent run failed after an applied write, however, the transaction rollback restored the previous OpenVMS file version instead of leaving a half-applied repair. That rollback behavior became one of the strongest positive findings of the milestone.

The mandated first target scaffold also revealed #54: `AGENT/WRITE` cannot naturally bootstrap a truly empty/nonexistent project because its write tools operate on existing roots/files. The immediate `AGENT/BUILD` then failed because no `BUILD.COM` existed, and `AGENT/REPAIR/PLAN` exposed #53: failed-build diagnostics were not available to the subsequent repair flow as expected. The target recovered through four guarded `AGENT/CREATE` operations for `storage.h`, `storage.c`, `main.c`, and `build.com`, rather than by pretending the original scaffold command had succeeded.

The first generated target build also demonstrated why VAX is the acceptance authority. A generated `time_t` check needed correction, and later target growth exposed further DEC C/OpenVMS assumptions that would not have been caught by source-only review.

## Phase 2 — target growth made architecture tooling real

Phase 2 added `fileutil`, `diff`, and `rules` modules through guarded creation. `AGENT/PLAN` produced a useful high-level expansion but plan persistence/scope handling itself showed friction (#55). Human confirmation remained valuable: several proposed writes were rejected because they introduced unwanted preprocessor structure or touched the wrong region. A recurring weakness was that rejecting a write did not reliably redirect the model; it could consume additional turns proposing variants of the same unwanted action.

That behavior became #58. In one decisive run, the correct bounded patch was approved and applied, but the agent continued proposing unrelated writes. The user rejected them, the run exhausted its turn budget, and the transaction manager rolled back the already-correct first change. #57 captured a related but distinct control-flow contradiction: after an approved write, the continuation request could force `tool_choice=required` even though the loop explicitly allowed a final text response. Strict providers rejected that request and triggered rollback. #57 was repaired and closed; #58 remains a design/transaction-finalization gap.

Target compilation exposed a generated portability defect using C99-style `SIZE_MAX`; DEC C/VAX acceptance required replacing it with a C89-compatible expression. Runtime testing then found an OpenVMS/RMS-specific semantic problem: copying text in binary `rb`/`wb` mode lost expected line structure. Switching the relevant target text path to text `r`/`w` mode produced the correct diff behavior. These were not OVMS Agent core defects, but they demonstrated that a generated program can look reasonable and still require actual VAX/RMS execution before it is trustworthy.

The required architecture pass then found product assumptions in the symbol index. #62 records that `REINDEX` hard-coded `SRC` and could not index the target's valid flat project root. After the flat-root behavior was repaired enough for M274 acceptance, #64 showed that module-path matching remained case-sensitive even though the OpenVMS target's indexed filename spelling and user command spelling differed. Both issues remain useful evidence even though the milestone was able to continue with accepted repaired behavior.

Once the index represented the real project correctly, the graph was meaningful. The observed project edges included `diff.c -> fileutil.c`, `main.c -> storage.c`, `main.c -> diff.c`, `main.c -> rules.c`, and `rules.c -> fileutil.c`. `CLUSTER` reported singleton strongly connected components and `CYCLE` found no dependency cycle. M274 therefore did not invent one.

`SHOW/STRUCT` also produced #65 by misidentifying a local struct variable as a definition. That reinforced a theme seen repeatedly in M274: parser/index features that look acceptable against the OVMS Agent repository can still fail when exercised against a structurally different real project.

## Phase 3 — the required refactor found real parser and verifier defects

Phase 3 first added genuine risk scoring to `main.c`: error and warning counts produce a bounded 0–100 score with LOW/MEDIUM/HIGH/CRITICAL labels. This was intentionally real application logic so the required refactor would improve design rather than exist only to exercise commands.

The exact mandated sequence was then attempted: `EXTRACT/FUNCTION`, `RENAME`, `MOVE/FUNCTION`.

The first extraction was blocked by #69 because the refactor eligibility scanner treated the colon in a normal ternary expression as a C label. The target code was not modified to dodge the bug; OVMS Agent was repaired and the same genuine extraction was retried. #69 was closed after the heuristic distinguished actual labels from the conditional operator.

Further real refactor friction exposed #72, #73, #74, #75, and #76 around flat-root VMS filespecs, verifier assumptions, external callers, line-boundary replacement behavior, and write/review fallback behavior. #72, #74, and #75 were repaired/closed during the milestone. #73 and #76 remain representative unresolved verifier/write-path gaps.

The final accepted refactor moved risk scoring out of `main.c` into `rules.c`, with the `report_risk_score` prototype in `rules.h`. The target remained DEC C/VAX compatible and runtime acceptance reported `Warnings: 0`, `Errors: 0`, `Risk score: 0/100 (LOW)` with success.

The important conclusion is not simply that three refactor commands can run. M274 showed that their usefulness depends on path canonicalization, C syntax classification, caller repair, OpenVMS filespec handling, and post-refactor build verification all agreeing on the same project model.

## Phase 4 — a real PR path also became an RMS/DCL test

Phase 4 required a meaningful branch and PR through `AGENT/GITHUB/PR`. The selected change fixed a real operational problem: #77 showed that repeated canonical builds accumulated OpenVMS `.OBJ`/`.EXE` file versions until the device filled and unrelated links failed. `BUILD.COM` was changed to purge generated `[.BUILD]` versions safely while retaining the latest version; repeated VAX builds confirmed the fix and #77 was closed.

The GitHub bridge itself required several iterations. The decisive lesson was RMS record format rather than HTTP semantics. A request body that looked correct when displayed could still reach GitHub incorrectly because ordinary OpenVMS file creation produced VFC record structure. Creating the JSON body as `STREAM_LF` and using curl `--data-binary @file` made the REST request work reliably.

The accepted agent command then performed the real operation:

`AGENT/GITHUB/PR create m274-agent-stress-project m274-phase4-build-purge 77 Bound-OpenVMS-build-artifact-versions`

It returned success, GitHub independently showed PR #80 with the intended base/head and issue relationship, and the PR was merged. The resulting parent checkpoint was `c82ba7a19ab928a9975b6ca38cda2c0d732707f4`, followed by a green VAX canonical build. #78 remains for bridge setup/documentation/service-surface completeness.

This phase was a useful positive result: once the OpenVMS file-format and DCL quoting problems were understood, the shipped agent GitHub PR route could perform a genuine end-to-end service action rather than merely print a URL or prepare a manual command.

## Phase 5 — provider agnosticism is a normalization problem, not just an endpoint problem

M274 switched to genuinely different provider backends and recorded observable differences rather than assuming Responses-compatible endpoints behaved identically.

Requesty/OpenAI-compatible routing was useful earlier in the milestone but also showed streaming/friction and later an account-restriction condition. Further paid Requesty use was deliberately avoided.

OpenRouter behavior varied by routed model. GPT-4o-mini could execute multi-turn tool loops but was prone to repeated/extra write proposals. North Mini Code was often better at small deterministic writes and successfully completed some repairs, but redundant post-write behavior still reproduced #58 and the free-model daily quota was eventually exhausted. Other free OpenRouter models demonstrated unavailable slugs, empty or intent-only responses, or output-token reservation problems such as #56. The `openrouter/free` route was useful for connectivity but not reproducible enough to be an acceptance baseline because the backend model can change.

Groq provided the clearest genuinely different backend. Basic `ASK` with `qwen/qwen3.6-27b` worked, and valid read tools could execute, but agentic compatibility exposed several distinct defects. #66/#67 captured tool-schema/name problems with `gpt-oss-120b`; #61 captured a provider incompatibility around `store:true`; #68 showed that a provider 429 with an explicit retry interval aborted the whole agent loop instead of retrying the same turn; #79 showed a Qwen `AGENT/CREATE` response shape that OVMS Agent recognized as neither output text nor a function call; and #81 showed the opposite failure mode, where valid assistant text containing only an intention to inspect files was accepted as a final result with zero tool evidence.

The provider comparison therefore did not identify a single universally "best" backend. It demonstrated that OVMS Agent's provider layer must normalize at least response-item shapes, tool-call schemas, tool names, optional request fields, output-token limits, finalization criteria, and rate-limit retry behavior. A common base URL/protocol is not enough.

## Phase 6 — persistent sessions worked, but their write capability was incomplete

Phase 6 intentionally spanned multiple sittings. Base session `42BABDB9` was forked into child `6F535C3D`. The child preserved its parent relationship, goal/history, and execution state across OVMS Agent exits and relaunches. `AGENT/SESSION/RESUME` restored the session; effective approval correctly reset to read-only and had to be reapproved to `workspace`; the write gate remained explicit. This was genuine process-level persistence, not a single-process pause/resume demonstration.

The session also accumulated real tool evidence. A stronger read-only prompt caused Groq Qwen to execute `list_directory` and `read_file`. Later provider turns hit the #68 TPM behavior, but the session and prior tool history remained inspectable. The first real reporting slice—`summary`—was then written through deterministic `AGENT/TOOL/RUN PATCH` operations while the persisted child was current. That code was built and run successfully on VAX before the final source was published.

Two limitations then changed the implementation route.

#82 showed that the resumed workspace continuation can edit existing files but cannot create a new file through the same persistent `AGENT/EXEC/RESUME` path because its advertised tool set lacks `create_file`. The original modular `report.c/report.h` design therefore could not be completed through the resumed session without abandoning the intended session workflow or switching to the separate `AGENT/CREATE` path, which itself had Groq/Qwen issue #79. The target was deliberately adapted to static helpers in existing `main.c` rather than pretending a new-file write had occurred.

#83 then exposed a lower-level deterministic tool defect. Top-level input supports roughly 8192 bytes, but `AGENT/TOOL/RUN` copies live arguments through the 768-byte transcript argument buffer before dispatch. Long `PATCH` text is silently truncated, producing misleading parser usage errors. More seriously, the same sanitizer replaces literal pipe characters with spaces, so a C null-check containing `||` reached the PATCH preview as missing operators and briefly produced invalid source. That defect is architectural: transcript/log sanitization must never mutate the live command being executed.

At that point the Phase 6 session requirement had already been genuinely satisfied: the child had been forked, resumed across multiple process launches/sittings, had real read/tool history, and had performed real guarded source writes that passed VAX build/runtime acceptance. Per repository rules, routine source editing therefore returned to GitHub instead of continuing to force production code through a now-known-broken deterministic path.

The final combined `summary`/`query` source commit was `8c29ef13bd49298c0b7c76613fd0b2b5c8175fa4` on `m274-phase6-reporting`. It changed only `m274_netcfg/main.c` (+128/-0) and was VAX-accepted at that exact head. PR #85 merged it to the M274 parent as `ba31fdbb91487243892472a0d836a6447fe79f71`.

Phase 6 sync produced one last real OpenVMS-path defect, #84. `GITRESTORE main.c` from the nested project failed because the tool uses one path both as the Git tree path (`HEAD:path`) and as the local RMS path. From the project directory those namespaces differ: local `main.c` corresponds to Git `m274_netcfg/main.c`. Running the restore from the repository root made the paths coincide and allowed the RMS-aware verified restore to work.

A native `git merge --ff-only FETCH_HEAD` also printed an apparent update while `git rev-parse HEAD` remained on the old commit. M274 therefore used explicit low-level ref/index operations plus RMS-aware restore and verified every postcondition. This reinforced an existing project rule: on this VAX, Git success text is not sufficient evidence that ref, index, and effective RMS working-file state all moved together.

## Safety behavior that held up well

The strongest positive finding was transactional rollback. Several provider/model failures occurred after a file version had been written, and the transaction manager restored the pre-run OpenVMS versions rather than leaving partially trusted source. That behavior prevented M274 from accumulating corruption while intentionally stressing unreliable model/provider paths.

Human approval gates also mattered. Incorrect or over-broad model proposals could be rejected before mutation. The milestone repeatedly relied on those previews to preserve scope.

RMS-aware `GITRESTORE` was valuable once used from a path where Git-tree and local-file namespaces matched. It not only rewrote the effective OpenVMS file version from `HEAD:path`; it re-read the local file and verified its content against the Git blob before reporting success. #84 limits where it works, but the verification property itself is good.

The architecture/refactor tooling also became useful once flat-root and path/case defects were understood. The eventual graph was coherent, the real extraction/rename/move sequence improved target design, and the post-refactor target remained VAX-green.

Finally, the GitHub PR service path proved capable of a real PR create after DCL/RMS handling was repaired. That is materially stronger evidence than a mock API test.

## Friction and failure modes that remain important

M274 produced more defect reports than a polished application milestone would, and that is a success criterion for this particular milestone. Representative unresolved or still-relevant findings include:

- #52: source-edit command handling can drop C `&&`-style operators; #83 later exposed the broader live-argument sanitizer class;
- #53: failed `AGENT/BUILD` diagnostics are not reliably available to the following `AGENT/REPAIR/PLAN` flow;
- #54: ordinary `AGENT/WRITE` cannot bootstrap an empty/nonexistent project as the Phase 1 scaffold expected;
- #55: plan persistence/scope handling can fail even when the model plan itself is reasonable;
- #56: OpenRouter-compatible output-token reservation can reject otherwise small requests;
- #58: a correct approved write can be lost because the model continues proposing writes and the transaction never reaches a clean final state;
- #61: provider request fields such as `store:true` are not universally accepted;
- #62 and #64: flat-root indexing and OpenVMS case-insensitive path semantics remain explicit symbol-index issue records;
- #65: `SHOW/STRUCT` can confuse local struct variables with definitions;
- #66/#67: provider-specific tool schema/name normalization failures;
- #68: transient 429 responses abort the agent loop instead of honoring a bounded retry interval;
- #70: some OpenRouter paths can serialize plausible tool calls as ordinary assistant text;
- #71: goal-guard behavior has a synthetic/NULL-state edge case;
- #73: refactor verification contains a `[.BUILD]` invalidation assumption that does not fit every flat-root target;
- #76: `AGENT/WRITE` can enter orphan/ranged-read/oversized-review fallback problems;
- #78: the GitHub bridge still needs complete setup/grammar/service-surface documentation;
- #79: Groq Qwen `AGENT/CREATE` response normalization failure;
- #81: tool-dependent goals can finalize on intent-only text with zero tool evidence;
- #82: a persistent resumed workspace cannot create new files through its continuation tool set;
- #83: live `AGENT/TOOL/RUN` arguments are incorrectly bounded/sanitized through a transcript buffer before dispatch;
- #84: `GITRESTORE` conflates repository-relative Git paths and current-directory RMS paths.

Several important defects were repaired and closed during the milestone rather than deferred: #50, #51, #57, #69, #72, #74, #75, and #77. The fact that M274 both found and repaired product defects while still progressing the target is evidence that self-hosting is useful, but it also means regressions must preserve the exact real-world conditions that exposed them.

## What the regression suite was missing

M274 suggests several concrete regression-coverage priorities.

First, project-root coverage must vary. A suite dominated by the OVMS Agent repository's `SRC` layout will miss flat-root projects, nested project roots, repository-relative versus current-directory path differences, and OpenVMS filename case behavior. Tests should execute from a real nested current default and assert ref, Git tree path, local RMS path, symbol-index path, and temporary-file location separately.

Second, write-loop regressions need end-to-end transaction scenarios, not only isolated parser tests. At minimum: read -> approved write -> final text; read -> approved write -> rejected extra write; provider failure after a write; turn-limit exhaustion after an approved write; and explicit proof that a valid human-approved change is either committed or rolled back for a documented reason.

Third, build/repair must be tested as one workflow. A regression that proves `AGENT/BUILD` prints an error is insufficient if `AGENT/REPAIR/PLAN` cannot retrieve that same failure afterward. The persisted diagnostic contract should be asserted across command boundaries.

Fourth, empty-project creation needs its own acceptance path. If `AGENT/WRITE` is not intended to create missing files, the scaffold workflow should route explicitly to bounded creation rather than advertise a capability that fails only when the target is genuinely empty.

Fifth, live tool arguments and transcript storage must be separated. Transcript/log size limits and character sanitization may bound persisted observability, but they must never modify the string sent to the executing tool. Regression coverage should include payloads larger than 768 but smaller than the top-level input limit and operators containing `|`, `||`, `&`, and `&&`.

Sixth, persistent-session capability parity should be explicit. A resumed workspace session should either have a confirmation-gated create capability or clearly advertise that it can only edit existing files. A regression should fork, exit the process, relaunch, resume, reapprove workspace policy, create or edit as supported, and verify transcript/history continuity.

Seventh, provider compatibility needs fixtures for multiple valid Responses shapes and failure classes: ordinary text, tool calls, tool calls serialized as text, unknown output item types, optional unsupported request fields, max-output-token constraints, empty output, intent-only text, transient 429 with retry-after, and strict `tool_choice` enforcement. Provider behavior observed in M274 differed enough that a single mock-provider shape is not adequate.

Eighth, refactor regressions should include real C syntax and project topology: ternary expressions, labels, external callers, flat-root VMS filespecs, case variants, and post-refactor object invalidation. A successful preview is not enough; the regression should build and verify the resulting target.

Ninth, OpenVMS Git/RMS synchronization needs postcondition checks. A command printing `Updating old..new` is not acceptance. Tests and operational flows should verify the intended ref SHA, index state, effective RMS file content, and clean tracked status independently.

## M274 sequencing and acceptance

1. Repair the real nested-project Git-context blocker before generating target source. **Accepted; #50 closed.**
2. Generate the Phase 1 inventory scaffold through guarded agent flows, build immediately, and preserve the real empty-root/build-repair friction rather than hand-fixing it away. **Accepted with #53/#54 recorded and guarded-create recovery.**
3. Grow diff/rules modules and then run real project architecture analysis. **Accepted; graph/cluster/cycle exercised after #62/#64 findings; no cycle was manufactured.**
4. Add genuine risk-score logic and execute `EXTRACT/FUNCTION -> RENAME -> MOVE/FUNCTION` in that exact order. **Accepted after real refactor defects including #69/#72/#74/#75 were repaired or reconciled.**
5. Route a meaningful change through an actual OVMS Agent GitHub PR creation path. **Accepted; PR #80 created by `AGENT/GITHUB/PR` and merged.**
6. Switch to a genuinely different provider backend and record behavioral differences. **Accepted; OpenRouter/Requesty/Groq differences produced multiple concrete compatibility observations.**
7. Build the final reporting/query layer through a forked/resumed persistent session across more than one sitting. **Accepted; child `6F535C3D` survived multiple process launches, performed real source work, and final Phase 6 code was VAX-accepted and merged via PR #85.**
8. Reconcile what was actually exercised, what was not, what surprised positively/negatively, and what the gaps imply for regression coverage. **This document.**

## Final acceptance checkpoints

The Phase 4 parent checkpoint after PR #80 was `c82ba7a19ab928a9975b6ca38cda2c0d732707f4` and passed the canonical VAX build.

The Phase 6 source checkpoint `8c29ef13bd49298c0b7c76613fd0b2b5c8175fa4` was accepted at the exact child head with clean `git diff --check`, target build success, complete summary/query runtime acceptance, existing command regressions, and canonical root `All regression tests passed.` / `%X00000001`.

PR #85 merged that tree to the parent as `ba31fdbb91487243892472a0d836a6447fe79f71`. The child and merge commits were verified to have the same tree. The VAX then switched to the exact parent head and repeated target summary/query smoke tests plus the canonical root build. Final result: `TARGET_MERGE_STATUS=%X00000001`, `SUMMARY_MERGE_STATUS=%X00000001`, `QUERY_MERGE_STATUS=%X00000001`, `ROOT_MERGE_STATUS=%X00000001`, `All regression tests passed.`, exact branch `m274-agent-stress-project`, exact head `ba31fdbb91487243892472a0d836a6447fe79f71`, and no tracked modifications.

`ba31fdbb91487243892472a0d836a6447fe79f71` is therefore the M274 VAX-green code acceptance checkpoint described by this reconciliation. A later reconciliation-only commit is documentation state until separately materialized/accepted on VAX; it does not retroactively change the code checkpoint.

## Assessment

M274 succeeded because it was allowed to be messy. If the target had been pre-corrected, if provider failures had been replaced with manual edits, if a cycle had been manufactured for command coverage, or if GitHub had been used to bypass every uncomfortable agent path, the milestone would have produced a cleaner demo and much less useful evidence.

The strongest result is that OVMS Agent can already perform meaningful self-hosted work on OpenVMS: it can generate and edit source under human approval, recover safely through transactional rollback, build real DEC C code, analyze and refactor a nontrivial project, create a real GitHub PR, persist development sessions across process launches, and support a VAX-accepted end product.

The equally important result is that its reliability is still highly sensitive to path layout, provider response shape, transaction finalization, prompt/model compliance, OpenVMS RMS semantics, and assumptions hidden in utility commands. Many of those gaps were invisible until the agent was asked to develop a different project from a nested flat root over multiple sittings.

The next quality step should therefore not be broader command count for its own sake. It should be regression depth around the exact failure classes M274 exposed: nested/flat OpenVMS projects, end-to-end build-to-repair state, transactional continuation after writes, provider normalization and retries, session capability parity, live-versus-transcript argument separation, and explicit Git/ref/index/RMS postcondition verification.
