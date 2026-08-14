# OVMS Agent Practical Codex CLI Parity Specification

**Document status:** Authoritative parity specification, M254 reconciled
**Baseline date:** 14 August 2026
**Target platform:** OpenVMS V7.2 VAX and later
**Reference:** Current open-source Codex CLI behavior

---

# Purpose

This document defines the requirements for practical behavioral parity between
OVMS Agent and the current open-source Codex CLI.

Parity means that OVMS Agent can complete the core local repository-engineering
workflows expected from a modern coding agent. It does not require identical
commands, implementation details, user-interface conventions, or operating-system
semantics.

OpenVMS-native behavior is an adaptation, not a parity failure, when it preserves
the intent and safety of the corresponding workflow.

Practical parity is achieved only when every required capability and acceptance
test is **VERIFIED**.

---

# Parity principles

1. Parity is based on observable behavior and outcomes.

2. Claims must be supported by source code, repeatable commands, automated tests,
   build logs, or recorded regression results.

3. OVMS Agent does not need to reproduce Codex CLI command names.

4. OpenVMS-native mechanisms are preferred over artificial POSIX emulation.

5. Safety boundaries must be explicit and testable.

6. A capability is not VERIFIED merely because a command or function exists.

7. A capability that works only for a subset of its required workflow is PARTIAL.

8. Unsupported behavior is MISSING, not silently excluded.

9. Unevaluated behavior is UNKNOWN.

10. NOT_APPLICABLE may be used only when the capability is not required for
    practical local coding-agent parity and the reason is documented.

11. Tests must verify both successful operation and relevant failure behavior.

12. Documentation must describe the implementation that actually exists.

---

# Evidence rules

A VERIFIED capability must have reproducible evidence containing:

* the command, prompt, or operation performed;
* the files or project state used as input;
* the expected result;
* the observed result;
* the final OpenVMS condition status when applicable;
* confirmation that unrelated files were not modified; and
* a reference to the source module, test image, build log, or regression record.

Evidence may include:

* automated test images;
* BUILD.COM output;
* OVMS_AGENT_ACTIVITY.LOG;
* OVMS_AGENT_BUILD.LOG;
* command transcripts;
* source-level inspection;
* Git diffs;
* file-version inspection;
* RMS attribute inspection; and
* pre-operation and post-operation fingerprints.

A capability must not be marked VERIFIED solely from model-generated prose.

---

# Status legend

## VERIFIED

The complete required behavior has been demonstrated by reproducible evidence and
is reliable for normal use.

## PARTIAL

A useful implementation exists, but one or more required behaviors, safety
properties, interfaces, or regression tests remain incomplete.

## MISSING

The required behavior is not implemented.

## UNKNOWN

The behavior has not been evaluated sufficiently to classify it.

## NOT_APPLICABLE

The behavior is outside the practical parity scope. A written rationale is
required.

Any required item with status PARTIAL, MISSING, or UNKNOWN prevents practical
parity from being declared complete.

---

# OpenVMS native adaptations

The following are OpenVMS-native adaptations and are not parity failures by
themselves.

## RMS text files

OVMS Agent must preserve the RMS organization, record format, record attributes,
and carriage-control behavior of files it replaces.

A POSIX byte-stream representation is not required when an OpenVMS record-oriented
representation preserves the intended text.

## File versions

OpenVMS file versions are part of the normal file model.

Creating a new file version during a guarded replacement is acceptable and often
preferred. Rollback must restore the correct original version and content.

## OpenVMS condition values

OpenVMS odd condition values represent success.

OVMS Agent must evaluate condition values according to OpenVMS rules rather than
assuming that only numeric zero represents success.

## DCL command procedures

BUILD.COM and other DCL command procedures are first-class build, test, and
workflow artifacts.

Executing BUILD.COM is the OpenVMS-native equivalent of invoking a repository
build or test command.

## Logical names and symbols

Logical names and DCL symbols may be used where other platforms use environment
variables, shell aliases, or executable search paths.

## Project-relative tool paths

Model-facing tools may use project-relative paths such as:

```
SRC/COMMAND.C
DOC/CODEX_PARITY.MD
```

The OVMS Agent path layer is responsible for resolving them safely to OpenVMS
device and directory syntax.

## Transactional replacement

OpenVMS file versions, temporary files, RMS metadata, and rename operations may
be used to provide atomic replacement and rollback behavior.

The required parity property is a correct all-or-nothing outcome, not a particular
POSIX filesystem sequence.

---

# Capability matrix

## Core repository inspection

| ID      | Capability                         | Status   | Current evidence                                          |
| ------- | ---------------------------------- | -------- | --------------------------------------------------------- |
| CAP-001 | Project listing and tree traversal | VERIFIED | LIST, LIST/ALL, TREE, project_list, and project_tree      |
| CAP-002 | File reading and literal search    | VERIFIED | READ, SEARCH, read_file, read_file_range, and search_file |
| CAP-003 | Read-only agent analysis           | VERIFIED | CHAT, ASK, REVIEW, and read-only tool exposure            |

## File modification

| ID      | Capability                           | Status   | Current evidence                                                                 |
| ------- | ------------------------------------ | -------- | -------------------------------------------------------------------------------- |
| CAP-004 | Exact-text replacement               | VERIFIED | replace_text and guarded PATCH/EDIT workflows                                    |
| CAP-005 | Exact line-range replacement         | VERIFIED | replace_lines with verified inclusive ranges                                     |
| CAP-006 | Direct new-file creation             | VERIFIED | AGENT/CREATE and create_file                                                     |
| CAP-007 | Transactional multi-file replacement | VERIFIED | AGENT/EXECUTE and edit_txn                                                       |
| CAP-008 | Planned file creation                | VERIFIED | M150-M150D saved-plan create_file parsing, approval, commit, rollback, tamper tests |
| CAP-009 | File deletion                        | VERIFIED | M252.1 exact-version delete plus M253 RMS/isolation and automatic-failure rollback |
| CAP-010 | File rename                          | VERIFIED | M252.2 exact-version same-directory rename plus M253 preservation/failure evidence |
| CAP-011 | File move                            | VERIFIED | M252.3 exact-version cross-directory move plus M253 preservation/failure evidence |

## Planning and approval

| ID      | Capability                        | Status   | Current evidence                                                |
| ------- | --------------------------------- | -------- | --------------------------------------------------------------- |
| CAP-012 | Saved plans and file fingerprints | VERIFIED | OVMS_AGENT_PLAN.TXT and plan validation                         |
| CAP-013 | Explicit write approval           | VERIFIED | AGENT/APPROVE and session approval state                        |
| CAP-014 | Execution dry-run                 | VERIFIED | AGENT/EXECUTE/DRY_RUN                                           |
| CAP-015 | Guarded workspace-write workflow  | VERIFIED | M254 read-only/workspace/full policy and write/DCL gate regression evidence |

## Build, execution, and repair

| ID      | Capability                       | Status   | Current evidence                                                            |
| ------- | -------------------------------- | -------- | --------------------------------------------------------------------------- |
| CAP-016 | BUILD.COM execution              | VERIFIED | run_build, BUILD, and automatic build regressions                           |
| CAP-017 | OpenVMS status handling          | VERIFIED | Odd-status success handling in build workflows                              |
| CAP-018 | Transaction rollback             | VERIFIED | edit_txn rollback, M154-M159, M252, and M253 failure regressions            |
| CAP-019 | Iterative edit-build-test repair | PARTIAL  | AGENT/FIX and AGENT/RETRY exist; autonomous convergence remains incomplete  |
| CAP-020 | Arbitrary approved DCL execution | VERIFIED | M254 explicit DCL command with full/write/DCL gates, bounded output, logging, and exact condition capture |
| CAP-021 | Non-interactive agent execution  | VERIFIED | M254 one-shot @OVMS_AGENT DCL JSON mode with deterministic policy denial and exact OpenVMS process status |

## Sessions, policy, and extensibility

| ID      | Capability                         | Status   | Current evidence                                                  |
| ------- | ---------------------------------- | -------- | ----------------------------------------------------------------- |
| CAP-022 | Persistent state and activity logs | VERIFIED | OVMS_AGENT.STATE and OVMS_AGENT_ACTIVITY.LOG                      |
| CAP-023 | Resumable interactive sessions     | PARTIAL  | Durable state exists; complete task resumption is not implemented |
| CAP-024 | Project instruction files          | MISSING  | Root-level support exists later; full directory-scoped CP-019 remains unverified |
| CAP-025 | Configurable network policy        | MISSING  | No allow, deny, or approval-based outbound policy                 |
| CAP-026 | Image input                        | MISSING  | No image ingestion or image reasoning interface                   |
| CAP-027 | External tool extensibility        | MISSING  | External-tool work exists later; full capability requires separate reconciliation |

---

# Acceptance tests CP-001 through CP-020

Each test has an objective pass condition. A VERIFIED status requires recorded,
repeatable evidence.

## CP-001 - Project listing and tree traversal

**Status:** VERIFIED

**Pass condition:** LIST, LIST/ALL, and TREE enumerate the expected project
contents without leaving the project root or misinterpreting OpenVMS paths.

**Evidence:** Command registration, project_list, project_tree, and successful
interactive use.

## CP-002 - Exact file reading

**Status:** VERIFIED

**Pass condition:** READ and read_file return the intended project file without
modifying it.

**Evidence:** Interactive reads of project source and documentation files.

## CP-003 - Literal file search

**Status:** VERIFIED

**Pass condition:** SEARCH and search_file find known literal occurrences and
report their correct locations without false replacements.

**Evidence:** Search operations used during source inspection and guarded edits.

## CP-004 - Read-only analysis

**Status:** VERIFIED

**Pass condition:** A read-only analysis session may inspect and discuss the
repository but cannot invoke project write tools.

**Evidence:** Read-only agent command and tool registration behavior.

## CP-005 - Exact-text edit

**Status:** VERIFIED

**Pass condition:** An exact-text replacement changes only the intended unique
text and rejects an absent, ambiguous, or invalid match.

**Evidence:** PATCH, EDIT, replace_text, and associated guarded-write behavior.

## CP-006 - Exact line-range edit

**Status:** VERIFIED

**Pass condition:** A line-range replacement changes only the requested inclusive
range after the original lines have been verified.

**Evidence:** replace_lines implementation and guarded execution behavior.

## CP-007 - Direct file creation

**Status:** VERIFIED

**Pass condition:** AGENT/CREATE creates a previously nonexistent project file
only after displaying the proposed content and receiving approval.

**Evidence:** Creation of DOC/CODEX_PARITY.MD through create_file.

## CP-008 - Atomic multi-file transaction

**Status:** VERIFIED

**Pass condition:** A planned multi-file execution either completes all intended
replacements or restores every original file.

**Evidence:** edit_txn implementation and transaction regression tests.

## CP-009 - Plan fingerprints

**Status:** VERIFIED

**Pass condition:** A saved plan records the identity or fingerprint of each
input file and rejects execution when a relevant file has changed unexpectedly.

**Evidence:** OVMS_AGENT_PLAN.TXT, validation commands, and stale-plan handling.

## CP-010 - Approval gating

**Status:** VERIFIED

**Pass condition:** A guarded write plan cannot execute until the current session
contains explicit approval for that plan.

**Evidence:** AGENT/APPROVE and AGENT/EXECUTE behavior.

## CP-011 - Dry-run execution

**Status:** VERIFIED

**Pass condition:** AGENT/EXECUTE/DRY_RUN reports the intended operations without
changing project files or creating committed output versions.

**Evidence:** Dry-run command path and plan execution controls.

## CP-012 - BUILD.COM execution

**Status:** VERIFIED

**Pass condition:** run_build executes BUILD.COM, captures the result, and treats
an odd OpenVMS condition value as success.

**Evidence:** Repeated successful builds ending with `%X00000001`.

## CP-013 - Rollback after failure

**Status:** VERIFIED

**Pass condition:** An injected edit or build failure leaves the original file
contents, versions, and required RMS attributes intact.

**Evidence:** M154-M159 rollback/RMS regressions plus M253 delete/rename/move RMS/isolation and automatic write-failure rollback evidence.

## CP-014 - Persistent state

**Status:** VERIFIED

**Pass condition:** Required durable agent state survives separate command
operations and can be reloaded from OVMS_AGENT.STATE.

**Evidence:** State subsystem and status commands.

## CP-015 - Activity logging

**Status:** VERIFIED

**Pass condition:** Significant agent operations and outcomes are recorded in
OVMS_AGENT_ACTIVITY.LOG with enough information for later review.

**Evidence:** Existing activity log subsystem and log-display commands.

## CP-016 - Planned file creation

**Status:** VERIFIED

**Pass condition:** A saved multi-step plan may contain one or more new-file
operations, display their full proposed contents, validate that the paths remain
nonexistent, and create them transactionally during approved execution.

**Evidence:** M150-M150D saved-plan create_file parser, path/preflight validation, approval gating, transactional success, build-failure rollback, tamper rejection, and transaction failure regressions.

## CP-017 - Iterative repair

**Status:** PARTIAL

**Pass condition:** The agent can inspect a failed build, produce a bounded repair,
execute it after required approval, rebuild, and repeat until success or a defined
attempt limit is reached.

**Current evidence:** AGENT/FIX, AGENT/RETRY, build execution, and rollback pieces
exist, but the complete autonomous loop is not verified.

## CP-018 - Resumable sessions

**Status:** PARTIAL

**Pass condition:** After exiting and restarting OVMS Agent, the user can resume
an unfinished task with its plan, approval state, completed steps, pending steps,
and relevant context intact.

**Current evidence:** Durable state and plan files exist, but complete interactive
task resumption is not implemented.

## CP-019 - Project instructions

**Status:** MISSING

**Pass condition:** OVMS Agent automatically discovers project instruction files,
applies root and directory-specific instructions, reports which instructions are
active, and enforces them during planning and execution.

**Current evidence:** Root-level instruction support exists in later work, but directory-specific scope and the complete acceptance condition have not been verified.

## CP-020 - Image input and external tools

**Status:** MISSING

**Pass condition:** OVMS Agent can accept an image as task context and can invoke
registered external tools through an explicit, inspectable, policy-controlled
interface.

**Current evidence:** External-tool support exists in later work, but image ingestion remains missing and the combined acceptance condition is not satisfied.

---

# Milestone roadmap

The roadmap below groups remaining parity work. A group may be divided into
smaller implementation milestones when necessary.

## Phase 1 - Evidence hardening

1. Create an automated parity-test driver.
2. Map every VERIFIED capability to an executable regression.
3. Store concise evidence references in this document.
4. Reclassify any item whose evidence does not support VERIFIED status.

## Phase 2 - Complete guarded file operations - COMPLETE

Validated through M150-M150D, M252.1-M252.3, and M253 OpenVMS evidence.

1. Plan-driven file creation - complete.
2. Guarded deletion with exact-version rollback - complete.
3. Guarded rename and move operations - complete.
4. RMS attributes and version semantics preserved with focused evidence - complete.
5. Failure-injection regressions for each structural operation - complete.

## Phase 3 - Complete agent execution modes - COMPLETE

Validated through M254.1-M254.3 OpenVMS evidence.

1. Explicit read-only, workspace-write, and full policies - complete.
2. Approved generic DCL command execution - complete.
3. Command execution constrained by policy plus write/DCL gates - complete.
4. Bounded output and exact OpenVMS condition capture - complete.
5. Non-interactive one-shot DCL entry point - complete.
6. Deterministic denial status plus machine-readable JSON output - complete.

## Phase 4 - Iterative engineering loop

1. Connect diagnostic parsing to repair planning.
2. Add bounded edit-build-test iteration.
3. Stop on success, repeated failure, unsafe action, or attempt limit.
4. Preserve rollback guarantees across iterations.
5. Produce an execution summary and final diff.

## Phase 5 - Session resumption

1. Persist task identity and user objective.
2. Persist active plan and completed steps.
3. Persist pending approvals without incorrectly reusing stale approval.
4. Detect changed files before resumption.
5. Add resume, list-session, and abandon-session operations.

## Phase 6 - Project instructions and configuration

1. Define an OpenVMS-friendly project instruction filename.
2. Support root and directory-specific instruction scope.
3. Define precedence and size limits.
4. Display active instructions.
5. Add tests proving that instructions affect planning and execution.

## Phase 7 - Network and external tools

1. Define network-disabled behavior as the default.
2. Add explicit domain allow and deny policies.
3. Add approval for policy exceptions.
4. Define a registered external-tool schema.
5. Add lifecycle, timeout, output-size, and condition-status rules.
6. Evaluate an MCP-compatible transport or an OpenVMS-native equivalent.
7. Log all external-tool and network-policy decisions.

## Phase 8 - Image input

1. Define supported OpenVMS image file formats and size limits.
2. Add a safe upload or local-file ingestion path.
3. Include image references in agent requests.
4. Record image metadata without placing binary data in text logs.
5. Add image-assisted coding acceptance tests.

## Phase 9 - Final parity validation

1. Run CP-001 through CP-020 on a clean checkout.
2. Run all rollback and RMS-format regressions.
3. Run BUILD.COM successfully.
4. Confirm final status `%X00000001`.
5. Confirm every required capability is VERIFIED.
6. Confirm no undocumented MISSING or UNKNOWN items remain.
7. Record the validation date, OpenVMS version, architecture, compiler version,
   model, and test operator.
8. Tag the validated commit.

---

# Definition of done

Practical Codex CLI parity is complete only when all of the following are true:

1. Every required CAP item is VERIFIED.

2. CP-001 through CP-020 are all VERIFIED.

3. Every VERIFIED status has reproducible evidence.

4. All automated parity, transaction, rollback, RMS-format, and build tests pass.

5. BUILD.COM completes with an odd OpenVMS success condition.

6. No required capability remains PARTIAL, MISSING, or UNKNOWN.

7. Every NOT_APPLICABLE classification has an approved written rationale.

8. Read-only and write-capable operation boundaries are explicit and enforced.

9. File operations preserve required OpenVMS file versions and RMS attributes.

10. Failed multi-file operations leave no partially applied project state.

11. Approved command execution is bounded, logged, and reports condition values.

12. Network and external-tool access follow explicit policy.

13. Project instructions are automatically discovered and enforced.

14. Interrupted work can be resumed without silently using stale plans or stale
    approvals.

15. User-facing documentation matches the implemented commands and behavior.

16. A clean-checkout validation transcript is stored with the project.

17. The validated commit is tagged as the practical-parity baseline.

Until every criterion above is satisfied, OVMS Agent may report progress toward
practical parity but must not claim complete Codex CLI parity.