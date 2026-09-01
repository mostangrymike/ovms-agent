# OVMS Agent Project Rules

This file records **current durable operating rules** for OVMS Agent. Milestone-specific history belongs under `DOC/`; completed milestone instructions should not remain here as though they were current workflow.

When a newer explicit user instruction conflicts with this file, the newer instruction wins. Update this file after the corrected rule becomes durable.

## 1. Source of truth and responsibility split

- **GitHub is the development control plane and repository source of truth.** Use it for committed file contents, repository discovery, branch/ref state, searches, diffs, edits, issues, pull requests, history, and other repository operations whenever the capability exists.
- **OpenVMS is the native acceptance authority.** The current canonical host is **OpenVMS V9.2-3 x86-64**; use it for live compiler/linker behavior, RMS semantics/record formats, DCL behavior, regressions, runtime behavior, and generated/uncommitted files.
- Historical DEC C/VAX compatibility remains a source/linker constraint where applicable; do not confuse that compatibility contract with the current canonical acceptance architecture.
- Make source/repository edits on GitHub by default. OpenVMS-side editing is an exception for RMS-specific acceptance work or when a safe GitHub mutation path is genuinely unavailable/impractical.
- Track significant milestones, defects, gaps, and durable project work with GitHub issues.
- Do not ask the user to repeat diagnostics or perform repository discovery that available GitHub tools can perform directly.
- Do not mix OVMS Agent build architecture with unrelated projects.

## 2. GitHub search and edit discipline

- Code-search results are **candidate discovery only**; the search index may be stale.
- Fetch exact active-branch file contents before making a correctness decision from a search result.
- Before a whole-file replacement, fetch the exact live file and blob SHA, change only the intended content, then compare the resulting branch head/diff.
- Audit direct GitHub edit batches before relying on them.
- Historical/archival files should not drive production migration decisions unless evidence shows they are live build/runtime inputs.

## 3. OpenVMS / DEC C compatibility

- Treat the historical DEC C/VAX **31-character externally visible identifier limit as a hard compatibility constraint**.
- Verify newly introduced external functions, global variables, and linker-visible names are 31 characters or fewer.
- Never rely on compiler/linker name truncation.
- Avoid implicit function declarations.
- Keep VAX-facing C compatible with the project's C89/DEC C discipline unless a specific existing subsystem demonstrably requires otherwise.
- Preserve existing compiler modes unless a deliberate change requires updating them.

## 4. Naming and repository layout

- Repository filenames are uppercase by default; root-level filenames must be fully uppercase, including extensions, unless an external/tooling constraint makes that impossible.
- `DOC` is the canonical documentation-history directory.
- Generic architecture/user-visible naming uses **LLM** or **provider**, not **OpenAI**, when behavior is provider-neutral.
- Preserve `OpenAI` only for genuinely OpenAI-specific protocol/provider/compatibility behavior.
- Preserve Requesty and other OpenAI-compatible intermediary compatibility.

## 5. Command presentation

- When giving commands to paste **inside OVMS Agent**, omit the literal `OVMS-AGENT>` prompt prefix.
- For EDT line-mode instructions, omit the leading `*` prompt marker.
- DCL examples may include `$` when useful.
- Prefer exact, pasteable commands and concrete next steps.

## 6. Git authentication on OpenVMS

- Use `GITAUTH` as the standing prompt-free authenticated raw Git network path. It already invokes Git; do not write `GITAUTH GIT ...`.
- Install/configure it with `@GIT_AUTH_SETUP`; load it on later logins with `@SYS$LOGIN:OVMS_AGENT_GIT_AUTH.COM` after normal Git setup.
- Keep `GIT_TERMINAL_PROMPT=0`; authentication must fail closed rather than falling back to interactive username/PAT prompts.
- Do not use plaintext `credential.helper=store` or the abandoned askpass/global-helper experiments as the standing solution.
- `GITAUTH --check` is the operator-facing **live write-authorization preflight**. It uses an authenticated non-mutating dry-run push against the active profile/current origin. `GIT_AUTH_SETUP.COM` uses the config-only check so installation itself does not depend on network access.
- Never expose the credential value in DCL symbols, command lines, logs, or documentation.
- An odd/native-looking status alone does not prove a network operation occurred. Require visible network/ref evidence and independently verify mutations on GitHub.

## 7. Git/RMS working-tree safety

- Keep `core.filemode=false`.
- Native OpenVMS Git porcelain/ref/index/worktree behavior can be misleading on RMS files. Do not assume `pull`, `reset`, `checkout`, `restore`, `add`, `status`, `rev-parse`, or `$STATUS` alone proves the intended state.
- Consult `DOC/OPENVMS_GIT_QUIRKS.MD` before inventing a new OpenVMS Git/materialization workflow.
- Use RMS-visible `.git` control files and physical files as independent evidence when applicable, including `[.^.GIT]FETCH_HEAD.`, `[.^.GIT]HEAD.`, branch-ref files, and `[.^.GIT]INDEX.`.
- Use the already validated `read-tree` + `update-ref` + `symbolic-ref` recovery path only for the documented failure mode. Those operations establish metadata; they do not materialize the working files.
- Use `GIT_RESTORE.COM` for RMS-aware tracked-file materialization. Batch multiple paths in short comma-separated lists; if `%DCL-W-TKNOVF` occurs, assume that batch did not reliably restore the files and split it.
- Selective restore can leave a hybrid tree. Check for stale prior-branch modifications even when they are outside the candidate's changed-file list.
- Repository deletion does not prove all physical OpenVMS file versions are gone; verify stale versions when deletion matters to acceptance.
- A failed compile can leave an older object available to LINK. Do not treat a later successful LINK as proof that the failed source produced a new object.
- `git hash-object` without `-w` computes but does not store a blob. Use low-level index operations only with independently verified SHAs/modes.

## 8. OpenVMS condition-status semantics

- Do not apply the generic odd/even native-status rule before decoding the project's encoded POSIX-exit status range.
- `OVMS_POSIX_EXIT_BASE` is `0x0035A000UL`; `OVMS_POSIX_EXIT_MASK` is `0x0FFFF800UL`.
- Encoded POSIX nonzero exits are failures even when the resulting OpenVMS status is odd. For example `%X0035A401` represents POSIX exit 128 and is failure; `%X1035A00A` represents POSIX exit 1 and is failure.
- `%X00030001` is a native odd success example; `%X0F0180EA` is an even curl-timeout failure example.
- `ovms_status_propagate` maps POSIX nonzero to native even failure status 2.
- Never call `%X0035A401` success.

## 9. Change-slice discipline

- Keep changes bounded and coherent; do not turn one defect/milestone into unrelated cleanup unless the extra work blocks correctness or acceptance.
- Prefer several small branch-verified edits in a coherent batch when that safely reduces native round trips.
- Keep compatibility shims until all live consumers are proven migrated and the replacement has passed acceptance.
- Separate provider/model compatibility gaps from generic deterministic/reliability defects unless one truly blocks the other.
- Generic agent architecture should remain provider-neutral; do not hard-code behavior learned from one LLM vendor as a universal assumption.

## 10. Build and acceptance

- GitHub correctness alone is not native acceptance.
- Source/build/runtime milestones require the canonical OpenVMS build/tests appropriate to the changed surface. The established full-build success evidence is:

  ```text
  All regression tests passed.
  Build completed successfully.
  BUILD_STATUS = "%X00000001"
  ```

- Focused regressions supplement, not silently replace, a required full build.
- Documentation-only changes that do not feed compilation/linking/launcher/runtime/HELP parsing may use a narrower documentation validation, with the reason recorded.
- Record the exact candidate SHA/tree that was actually native-tested.
- **Exact-SHA acceptance and tree-equivalent promotion are distinct facts.** Do not call an unbuilt GitHub merge SHA native-green merely because its tree equals an accepted candidate. State that the promoted merge carries the exact accepted tree.

## 11. Diff, commit, and promotion hygiene

- Inspect the changed/staged file set and diff before committing/promotion.
- Confirm there are no unintended files, mode changes, or broad formatting changes.
- After GitHub edits/deletes, compare previous/new heads.
- Promotion PRs should use an expected-head guard when available.
- Verify the resulting promoted `main` and tree independently after merge.

## 12. Milestone tracking and durable state

- Use GitHub issues as the durable issue/work tracker.
- Maintain `CHAT_SESSION_STATE.json` **only on `chat-session-state`** as cross-chat state.
- Update durable state only at meaningful full-green milestone checkpoints, not for every intermediate issue/commit.
- At chat/resume start, read current `RULES.MD` on `main` and the durable state, then verify live GitHub refs/issues before acting.
- Never label an untested GitHub candidate as the latest native-green state.

## 13. AUTOPILOT safety floor

- `AGENT/AUTOPILOT` automates only bounded local workspace repair/build behavior through existing guarded planning/write/rollback/build/history primitives.
- AUTOPILOT must **never** satisfy `FULL` approval. PUSH, GitHub mutations, MCP/external work, arbitrary DCL, and other FULL-gated operations remain manual.
- `FULL` approval does not implicitly enable AUTOPILOT.
- One AUTOPILOT invocation consumes one cumulative `AGENT/AUTO/LIMITS` budget across nested iterations.
- AUTOPILOT stops only in an explicit terminal state (`CLEAN_BUILD`, `RETRY_BUDGET_EXHAUSTED`, or `GOAL_GUARD_REJECTED`) and leaves a human-review artifact.

## 14. Guarded write behavior

- Reuse established guarded/RMS-aware write primitives; do not introduce parallel writers without explicit architecture review.
- Ordinary `AGENT/WRITE` existing-file changes preserve the one-materialized-write safety invariant. The advertised existing-file primitive is one atomic `structured_patch`, which may contain multiple exact non-overlapping hunks for one file.
- `replace_text` and `replace_lines` remain supervised `AGENT/FIX` primitives; raising `auto_writes` does not allow sequential existing-file patches in ordinary `AGENT/WRITE`.
- This one-file atomicity does not imply cross-file atomicity; use `CHANGESET`/transaction machinery for coordinated multi-file work.
- Model-invoked `structured_patch` in `AGENT/WRITE` must pass local validation/preview and explicit local confirmation before writing.

## 15. Provider and transport diagnostics

- Provider/model capabilities vary; preserve provider-neutral architecture.
- `PROVIDER TEST` is a real bounded response-health diagnostic, not a tool-call capability benchmark. It must not persistently switch the active profile/model, expose credentials, or leave credential-bearing scratch versions behind.
- Provider transport requests remain bounded by the established timeout behavior so one dead endpoint cannot wait indefinitely.

## 16. Milestone branch retention

- Keep `main` and `chat-session-state` permanently.
- Keep the active milestone development branch while work is in progress.
- After promotion, retain only the **two most recently completed milestone development branches** for convenience.
- Create a permanent lightweight `mNNN` tag on the canonical promoted `main` commit before an older completed branch becomes eligible for pruning.
- Delete temporary audit/candidate/carrier/promotion branches after purpose completion and durable recording.
- Never delete a branch containing unique unpromoted work. Verify acceptance, issue completion, tree representation on `main`, tag, and durable SHA recording before pruning.
- Ref-only tag creation and remote branch deletion do not materialize OpenVMS source files; do not combine branch pruning with risky RMS source staging.

## 17. OpenVMS HELP documentation

- Keep root `OVMS_AGENT.HLP` synchronized whenever a user-visible command or persistent SETTINGS entry is added, removed, renamed, gains/loses options, or materially changes behavior.
- HELP topics must explain both **what** the feature does and **when/why** to use it.
- Keep HELP valid for loading into an OpenVMS HELP library on the canonical acceptance system.

## 18. Semantic releases and changelog

- Beginning with 1.0.0, public releases use semantic versioning `MAJOR.MINOR.PATCH`.
- Each public release gets `vX.Y.Z` in addition to the milestone `mNNN` tag on the same canonical promoted commit.
- Patch releases are backward-compatible fixes, minor releases backward-compatible features, and major releases intentional incompatible changes.
- A semantic release candidate that changes product/build/runtime inputs requires normal canonical OpenVMS acceptance plus startup/`VERSION` verification from the exact candidate.
- Keep `CHANGELOG.MD`, `README.MD`, install/build guidance, supported security-release information, and live version-bearing metadata synchronized with releases.

## 19. Explicitly preserved exceptions

- GAP-010 was explicitly discarded and must not be revived unless the user explicitly requests it.
- Branch `m287-groq-gptoss-responses` was explicitly assessed as semantically superseded by M294 and pruned; do not revive or retain it solely because its historical commits were once unique.