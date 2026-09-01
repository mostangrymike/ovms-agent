# DOC Directory Guide

`DOC` is the canonical documentation-history directory for OVMS Agent.

## Current operating guidance

`OPENVMS_GIT_QUIRKS.MD` is a **living operational guide**. Update it when later native evidence changes the accepted OpenVMS Git/RMS/DCL workflow.

## Historical milestone records

The following files are intentionally historical snapshots and should be read in the context of the milestone/baseline named in each document:

- `M262_RECONCILIATION.MD`;
- `M273_RECONCILIATION.MD` through `M279_RECONCILIATION.MD`;
- `PARITY_AUDIT_M252.MD` through `PARITY_AUDIT_M260.MD`; and
- `CODEX_PARITY.MD`, the parity specification frozen/reconciled through M260 with its stated August 2026 baseline.

Terms such as `current baseline`, `authoritative`, `VAX-green`, or `current Codex CLI` inside those files refer to the historical scope/date of that audit unless the file explicitly says otherwise. They are **not** the current OVMS Agent operating/release state.

Do not retroactively rewrite historical acceptance evidence solely to match later platform terminology or product behavior. If a historical statement was factually wrong even for its original milestone, correct it with an explicit note rather than silently modernizing it.

## Current product documentation

For current behavior use the root documents:

- `../README.MD` — overview and current status;
- `../FEATURES.MD` — current capability catalog;
- `../ARCHITECTURE.MD` — current subsystem model;
- `../INSTALL.MD` — installation/provider/GitHub setup;
- `../BUILD.MD` — product build and native acceptance;
- `../CONTRIBUTING.MD` — current engineering workflow;
- `../SECURITY.MD` — current security boundaries;
- `../CHANGELOG.MD` — promoted milestone/release history;
- `../RULES.MD` — current durable project rules; and
- `../OVMS_AGENT.HLP` — current command and persistent SETTINGS reference.

Current canonical native acceptance host: **OpenVMS V9.2-3 x86-64**. Historical DEC C/VAX compatibility remains part of the source contract where explicitly documented, and historical milestone files may accurately describe VAX acceptance performed at that time.
