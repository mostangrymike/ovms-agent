# M260 Phase 9 Final Parity Validation Audit

**Baseline:** canonical `main` merge `55243d87a87322798e4377f21be99e71e5563b68` (M259 post-merge validated)

## Purpose

Phase 9 is the final practical-parity validation gate. No product behavior is added by M260. The milestone aggregates the already-verified parity evidence into one clean whole-suite run and records the final environment and result.

## Authoritative Phase 9 checklist

The authoritative `doc/codex_parity.md` requires:

1. run CP-001 through CP-020 on a clean checkout;
2. run all rollback and RMS-format regressions;
3. run `BUILD.COM` successfully;
4. confirm final status `%X00000001`;
5. confirm every required capability is VERIFIED;
6. confirm no undocumented MISSING or UNKNOWN items remain;
7. record validation date, OpenVMS version, architecture, compiler version, model, and test operator; and
8. tag the validated commit.

## M260 driver

`BUILD_M260.COM` is an orchestration-only DCL procedure. It runs, in order:

1. `BUILD.COM`
2. `BUILD_M252.COM`
3. `BUILD_M253.COM`
4. `BUILD_M254.COM`
5. `BUILD_M255.COM`
6. `BUILD_M256.COM`
7. `BUILD_M257.COM`
8. `BUILD_M258.COM`
9. `BUILD_M259.COM`

The procedure stops on the first even OpenVMS condition value and preserves that failure as its exit status. A complete run exits with the final odd success condition.

`BUILD.COM` contains the broad historical regression baseline, including the earlier capability foundation. Focused milestone drivers begin at M252 in canonical root, so there is no separate `BUILD_M251.COM` to invoke.

## Final release-candidate validation

The exact-head M260 release-candidate run completed successfully on 15 August 2026.

Recorded environment:

- validation time: `15-AUG-2026 14:03:04.32`;
- OpenVMS version: `V9.2-3`;
- architecture: `x86_64`;
- node: `X86923`;
- test operator: `MIKE`;
- configured model: `gpt-5.1`;
- compiler: `VSI C x86-64 V7.7-003 (GEM 50Z9T) on OpenVMS x86_64 V9.2-3`.

Observed results:

- canonical `BUILD.COM` passed;
- M252 guarded delete/rename/move and parser/execution regressions passed;
- M253 RMS/isolation and structural rollback evidence passed;
- M254 execution-mode policy/status evidence passed;
- M255 bounded iterative-repair commit, rollback, retry-context, and attempt-limit evidence passed;
- M256 persistent/resumable-session, stale-plan refusal, reapproval, and cross-process restart evidence passed;
- M257 project-instruction scope, precedence, and PLAN/WRITE enforcement evidence passed;
- M258 MCP/network-policy/lifecycle evidence passed;
- M259 image-ingestion, multimodal serialization, command/logging evidence passed, with the prior live API image acceptance recorded in `doc/parity_audit_m259.md`;
- terminal M260 message reported `M260 Phase 9 automated parity suite passed.`;
- final `$STATUS` was `%X00000001`;
- `git status -sb` remained `## m260-final-parity...origin/m260-final-parity`.

The intentional failure cases printed by M252/M255 are regression stimuli, not suite failures; each enclosing focused driver reached its documented success result.

## Final parity state

After the successful M260 release-candidate run:

- CAP-001 through CAP-027 are VERIFIED;
- CP-001 through CP-020 are VERIFIED;
- every automated parity, transaction, rollback, RMS-format, network/external-tool, session, project-instruction, repair, and image-input driver invoked by M260 passed;
- no required capability remains PARTIAL, MISSING, or UNKNOWN;
- Phase 9 automated validation is COMPLETE;
- practical Codex CLI parity criteria are satisfied subject only to merging this evidence-only M260 branch, validating canonical `main`, and tagging that validated canonical commit.

## Compatibility

M260 introduces no C source, no new production object, and no linker-visible identifier. Therefore it introduces no new DEC C/VAX 31-character external-symbol risk.

Filename audit: `BUILD_M260.COM` and `doc/parity_audit_m260.md` do not collide by case with existing canonical paths.

## Post-merge/tag gate

The validated release tag must be created only after M260 is merged and `BUILD_M260.COM` is rerun successfully on canonical `main`. This ensures the tag identifies the exact canonical commit that carries the final validation driver and evidence record rather than the pre-merge feature head.
