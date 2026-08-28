# OVMS Agent Project Rules

This file records durable operating rules learned during development of OVMS Agent. These are standing project rules, not milestone history. Update this file whenever a new durable rule is established or an existing rule is corrected.

## 1. Source of Truth and Tooling

- **GitHub is the development control plane and repository source of truth.**
- **Always maximize use of GitHub.** Use the connected GitHub repository for repository discovery, committed file contents, branch state, searches, diffs, edits, deletes, commits, issues, pull requests, comments, labels, commit history, and CI/check metadata whenever the capability exists.
- **Make source-code and repository-file edits on GitHub by default.** Do not ask the user to perform routine source edits on OpenVMS when the same edit can be made safely through GitHub. VAX-side editing is an exception reserved for cases where GitHub editing is technically unsafe or unavailable, such as a genuinely impractical whole-file operation or an RMS-specific acceptance repair.
- **Track project work, milestones, defects, gaps, and meaningful checkpoints with GitHub issues.** Update the applicable GitHub issue as work progresses and create/use a GitHub issue when durable project work needs tracking.
- **Maximize useful work in every turn.** Complete as much safe, relevant work as possible before handing the task back to the user: use available tools, make authorized GitHub edits directly, perform branch-live verification, update durable tracking when appropriate, and batch coherent work to minimize unnecessary round trips. Do not stop early merely to ask for confirmation or manual work when the next safe steps can be completed directly.
- **OpenVMS is the hardware/OS acceptance environment.** Use it for DEC C/VAX compile and link behavior, RMS semantics and record formats, DCL behavior, runtime behavior, live acceptance, and generated or uncommitted files.
- Do not ask for a VAX-side `SEARCH` or similar repository-discovery command when GitHub can answer the question.
- A local/VAX blob SHA is an acceptance cross-check, not a substitute for GitHub repository discovery or branch state.
- Do not mix OVMS Agent build architecture with unrelated projects.

## 2. GitHub Search Discipline

- GitHub code-search results are **candidate discovery only**. The code-search index may be stale.
- Every plausible live consumer or build entry discovered by search must be fetched from the exact active branch before declaring a namespace or naming family drained.
- Exact branch-live file content overrides stale search snippets.
- For large files whose connector fetch is truncated, use blob SHA/blob fetch or another exact branch-live retrieval method rather than inferring unseen content.
- Before a GitHub whole-file update, fetch the exact live file and SHA first, change only the intended text, then compare the resulting commit/diff.
- Audit direct GitHub edit batches with a commit comparison before relying on them.

## 3. OpenVMS / DEC C / VAX Constraints

- Treat the DEC C/VAX **31-character external identifier limit as a hard constraint**.
- Before every milestone, fix, or package, verify every newly introduced externally visible function, global variable, and linker-visible symbol is 31 characters or fewer.
- Shorten identifiers proactively; do not rely on compiler truncation.
- Avoid implicit function declarations.
- Keep VAX-facing C compatible with C89 / DEC C conventions unless a specific existing subsystem demonstrably requires otherwise.
- Do not introduce modern C syntax casually into VAX-facing code.
- Include-only changes and call-site renames that introduce no new external symbols still require normal compile/link acceptance but do not create new linker-name risk.

## 4. Filename and Naming Conventions

- **Repository filenames should be UPPERCASE by default.** Use lowercase only when uppercase creates a concrete external, portability, or tooling blocker.
- Do not mass-rename unrelated historical files merely for case normalization during another milestone. Normalize opportunistically when files are otherwise being renamed, then perform a dedicated audit when appropriate.
- Generic architecture names should use **LLM**, not **OpenAI**, when the concept is provider-neutral.
- Preserve `OpenAI` naming only when it represents genuinely OpenAI-specific protocol, configuration, compatibility, or provider semantics.
- Naming cleanup must not remove OpenAI provider support.
- Preserve Requesty and other OpenAI-compatible provider compatibility while neutralizing generic architecture names.
- Generic user-visible wording should say `LLM` or `provider` rather than `OpenAI` when the behavior is provider-neutral.
- Generic linker/build artifact names should use `LLM`, not `OPENAI`.

## 5. Editing and Command Presentation

- When giving commands to paste **inside OVMS Agent**, omit the `OVMS-AGENT>` / `ovms-agent>` prompt prefix.
- For EDT line-mode instructions, do **not** include the leading `*` prompt marker before commands such as `SUBSTITUTE` or `FIND`.
- DCL examples may include the `$` prompt marker when useful.
- Prefer exact, pasteable commands and concrete next steps.
- Do not ask the user to repeat information or diagnostics already supplied.

## 6. Git on OpenVMS

- **Use the M271 `GITAUTH` wrapper as the standing prompt-free raw Git network path on VAX/OpenVMS.** Run `@SYS$LOGIN:OVMS_AGENT_GIT_AUTH.COM` after Git setup (or `@GIT_AUTH_SETUP` from an accepted checkout to install it), then use `GITAUTH` for GitHub network commands that need the active saved OVMS Agent GitHub profile.
- `GITAUTH` uses the same proven short-lived, repository-scoped `http.extraHeader` / `include.path` authentication mechanism as guarded `AGENT/GITHUB` operations. It must not place the credential value in the DCL symbol, Git command line, or logs.
- Keep `GIT_TERMINAL_PROMPT=0` for this path so authentication fails closed instead of falling back to interactive username/PAT prompts.
- Do **not** use `credential.helper=store`, the former global `credential.helper` experiment, or the abandoned askpass path as the standing VAX solution. Those approaches were disproved by direct remote verification.
- An odd OpenVMS Git status by itself is **not** proof that a network operation occurred. For read-side acceptance, require visible Git network/ref output. For mutations, require visible Git update output and then independently verify the resulting GitHub ref/state before proceeding.
- Before promoting from a shallow acceptance clone, unshallow it and verify ancestry. A shallow boundary can make a genuine fast-forward appear locally as a non-fast-forward push rejection.
- Native VAX Git worktree/index operations can be unreliable on RMS files. Do not assume `git pull`, `git reset`, or `git add` has materialized or indexed files correctly merely because it returned success.
- When native Git cannot hash or stage an RMS file reliably, verify the physical file, use `git hash-object`, and if necessary use lower-level index operations such as `git update-index --cacheinfo` only with verified blob SHAs and repository modes.
- `git hash-object` without `-w` computes a SHA but does not store a new blob in the object database. Use `git hash-object -w` when a new blob must be referenced by the index.
- Preserve existing repository executable-mode bits unless there is an intentional reason to change them.
- EDT can cause local mode drift. Normalize the Git index mode before committing when required.

## 7. RMS and Working-Tree Safety

- GitHub branch content remains authoritative when native Git cannot reliably hash or materialize OpenVMS RMS files.
- `git fetch` plus `git reset --mixed --no-refresh` may update index/ref state without reconstructing the working file correctly on OpenVMS.
- Use the guarded RMS-aware restore mechanism for changed tracked files when a branch update must be materialized safely.
- **When multiple Git restores are required, use the `GIT_RESTORE.COM` batch script instead of issuing multiple individual `GITRESTORE` operations.**
- **Keep each quoted comma-separated `GIT_RESTORE.COM` path list short enough to stay below DCL command-element limits.** If a long restore command triggers `%DCL-W-TKNOVF`, assume that batch did not materialize the requested files; split the same branch-live restore set into several smaller verified batches before deleting stale files or building.
- For branch-side deletions, explicitly remove stale OpenVMS file versions when necessary before acceptance builds.
- OpenVMS file versioning can leave stale physical versions behind even after repository deletion; verify physical deletion when the build must prove a compatibility shim is truly gone.
- Be alert for RMS record-format damage from generic write paths. Repository correctness does not substitute for live RMS acceptance.

## 8. Change-Slice Discipline

- Make naming migrations in **bounded, coherent families** rather than global blind replacements.
- Prefer several small, branch-verified GitHub edits in one coherent batch before asking for a VAX acceptance build, when that safely reduces round trips.
- Keep compatibility shims until all branch-live production consumers are proven migrated.
- Delete compatibility shims only after live consumers are drained and the neutral replacement has already passed acceptance.
- After shim deletion, physically remove stale VAX versions and run a full acceptance build.
- Historical/archival source files should not drive production migration work unless there is evidence they are compiled or otherwise live. Classify them explicitly during final survivor audits.
- Generated metadata/index files may contain stale names; determine whether they are live build/runtime inputs before letting them block a production migration.

## 9. Build and Acceptance Rules

- A naming family is not complete merely because GitHub diffs look correct.
- A family reaches a durable acceptance checkpoint only after the relevant OpenVMS build/tests pass.
- Use a full `@BUILD` for bounded-stage acceptance when the changed surface can affect compilation/linking or when a compatibility shim/file is removed.
- Record the final DCL status explicitly, for example:

  ```text
  WRITE SYS$OUTPUT "BUILD_STATUS=''$STATUS'"
  ```

- `%X00000001` is the expected successful status in established acceptance flows.
- Focused regression procedures should also report their final status when used as milestone evidence.
- Do not commit a consumer transition that references new build artifacts unless the new artifacts are also present in the same commit or already exist on the target branch.
- Do not declare a migration complete from search results alone; compiler/linker feedback on VAX is authoritative acceptance evidence for missed live consumers.

## 10. Diff, Commit, and Push Hygiene

- Before committing, inspect the staged file set and staged diff.
- Confirm there are no unintended files, mode changes, or broad formatting changes.
- Keep commits scoped to one coherent change family or corrective step.
- After a direct GitHub edit or deletion, compare the previous and new branch heads to confirm only intended changes occurred.
- When a VAX-tested local commit is pushed, verify the resulting GitHub branch state before deleting compatibility files or beginning the next family.
- Do not leave the branch in a state where build consumers reference files that do not exist in the same branch.

## 11. Milestone Tracking and Durable State

- **Use GitHub issues as the durable issue/work tracker for OVMS Agent.** Significant gaps, defects, milestones, and project work should be represented and maintained there rather than only in chat or local notes.
- GitHub issue **#31** is the tracking issue for M267 generic OpenAI-to-LLM naming cleanup.
- Update the tracking issue at meaningful, durable checkpoints rather than for every tiny edit.
- Maintain `CHAT_SESSION_STATE.json` on the `chat-session-state` branch as durable cross-chat state.
- Update durable chat state when a new VAX-green checkpoint or materially changed resume instruction has been established.
- Do not mark an untested GitHub head as `latest_vax_green_head`.
- The resume workflow should read durable state first, then verify the live GitHub branch and tracking issue before acting.
- **Maintain this `RULES.md` file whenever a new standing project rule is learned, corrected, or superseded.**

## 12. M267-Specific Rules

- M267 goal: eradicate **generic** `OpenAI` filenames and namespaces in favor of `LLM` while preserving genuinely provider-specific OpenAI semantics.
- No generic production filename should contain `OpenAI` at final acceptance.
- No generic production C namespace should contain `openai_` or `OPENAI_` at final acceptance.
- Generic linker/build artifact names must use `LLM`.
- Tests and internal fixtures should migrate unless they intentionally exercise provider-specific or compatibility behavior.
- Final M267 acceptance must classify remaining OpenAI occurrences as one of:
  - provider-specific and intentional,
  - compatibility-specific and intentional,
  - archival/historical and explicitly exempted,
  - generated and regenerated/justified,
  - or remaining naming debt to remove.
- Final M267 acceptance requires a full VAX `@BUILD`, all applicable regressions, and the normal DEC C/VAX identifier audit.

## 13. Known Project Backlog Rules

- Do not mix unrelated backlog fixes into a naming slice unless they block acceptance.
- Native Git/RMS checkout synchronization deficiencies are a separate technical backlog; use the established guarded restore workaround during milestone work.
- `PROJECT.C`/patch write-path RMS behavior is a separate backlog unless it directly blocks the current acceptance task.
- Build/test version accumulation in `.BUILD` can exhaust disk space. Clean generated EXE/OBJ versions when necessary; long-term build cleanup hardening remains separate work.
- Provider/model tool-use capabilities vary; the agent architecture should remain provider-agnostic rather than hard-coding assumptions from one LLM provider.

## 14. Superseded / Explicitly Rejected Work

- GAP-010 was explicitly discarded and must not be revived unless the user later explicitly requests it.

## 15. M268 AUTOPILOT Safety Floor

- `AGENT/AUTOPILOT` may automate only the existing local workspace repair/build path; it must reuse existing guarded planning, transactional write, rollback, goal-guard, build, history, and export primitives rather than creating a parallel writer or execution surface.
- The `AUTOPILOT` approval tier may auto-approve local workspace writes and controlled rebuilds only while the bounded AUTOPILOT controller is running.
- `AUTOPILOT` must **never** satisfy a `FULL` approval check. PUSH, GitHub writes, MCP/external operations, arbitrary DCL, and every other operation already gated at `FULL` remain manual and unavailable to AUTOPILOT. This is a hard floor, not a configurable default.
- `FULL` approval does not implicitly enable AUTOPILOT and must not be treated as a more automatic AUTOPILOT tier.
- One AUTOPILOT invocation must consume one cumulative `AGENT/AUTO/LIMITS` turn/write budget across all nested planning and repair iterations; nested workflows must not reset that outer ceiling.
- AUTOPILOT must always stop in an explicit terminal state: `CLEAN_BUILD`, `RETRY_BUDGET_EXHAUSTED`, or `GOAL_GUARD_REJECTED`.
- Every AUTOPILOT stop must produce a local human-review artifact containing exact repair candidates/edits, final working-tree diff, build evidence, and repair history. No later PUSH is implied or authorized by AUTOPILOT completion.

## 16. Milestone Branch Retention

- Keep `main` and `chat-session-state` permanently.
- Keep the active milestone development branch while work is in progress.
- After promotion, keep only the **two most recently completed milestone development branches** for convenient inspection; older completed milestone branches should be pruned.
- Create a permanent lightweight tag `mNNN` on each milestone's canonical promoted `main` commit before its development branch becomes eligible for deletion.
- Temporary audit, candidate, carrier, state, reconciliation-helper, and promotion-staging branches should be deleted once their purpose is complete and the accepted/promotion SHAs are recorded durably.
- Never delete a branch that contains unique unpromoted work. Before pruning, verify the milestone is accepted, its issue is closed/completed when applicable, the accepted tree is represented on `main`, and the important SHAs are recorded in durable state.
- Ref-only tag creation and remote branch deletion do not materialize or stage OpenVMS worktree files and therefore do not use the risky RMS checkout/index path; do not combine branch pruning with VAX source staging or publication.

## 17. OpenVMS HELP Documentation

- **Keep the root-level `OVMS_AGENT.HLP` synchronized with the live user interface.** Whenever a user-facing command or persistent SETTINGS entry is added, removed, renamed, gains or loses options, or materially changes behavior, update the corresponding HELP topics in the same change or milestone.
- Every command and setting topic in `OVMS_AGENT.HLP` must explain both **what the feature does** and **when or why a user would normally use it**; a syntax-only or one-line description is not sufficient.
- Keep `OVMS_AGENT.HLP` as valid OpenVMS HELP source that can be loaded into a HELP library on the canonical OpenVMS acceptance system.

## 18. Semantic Releases

- Beginning with OVMS Agent 1.0.0, public releases use semantic versioning in the form `MAJOR.MINOR.PATCH`.
- Use a patch release for backward-compatible fixes, a minor release for backward-compatible features, and a major release for intentional incompatible changes.
- Every public semantic release receives a permanent `vX.Y.Z` tag on the same canonical promoted `main` commit as its milestone `mNNN` tag; semantic release tags complement rather than replace milestone tags.
- A semantic release candidate that changes product/build/runtime inputs requires the normal canonical OpenVMS/VAX acceptance plus an explicit startup and `VERSION` check proving the intended semantic version from the exact candidate.
- Do not create or advertise the final semantic release tag until the milestone candidate has passed its required acceptance and has been promoted to canonical `main`.
- Keep `CHANGELOG.md`, `README.md`, installation/build guidance, supported security release information, and genuine live version-bearing metadata synchronized with the release in the same milestone.

---

When a conflict appears between this file and a newer explicit user instruction, the newer explicit instruction wins. Update this file to reflect the new standing rule once the correction is established.