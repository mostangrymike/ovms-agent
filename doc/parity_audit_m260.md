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

## Required preconditions

Before the final run:

- branch/checkout must be synchronized with its GitHub ref;
- `git status -sb` must show no local modifications or untracked validation artifacts relevant to the repository;
- the live M259 image fixture must not remain in the project root;
- API credentials/configuration used for the image acceptance must remain valid if any live evidence is repeated.

## Metadata

`BUILD_M260.COM` prints:

- validation time;
- OpenVMS version;
- architecture;
- node;
- test operator; and
- compiler version (`CC/VERSION`).

The configured OpenAI API model must be recorded from the final validation environment/transcript. M260 will not infer or guess that value from source.

## Current parity state before final run

After M259 reconciliation:

- CAP-001 through CAP-027 are VERIFIED;
- CP-001 through CP-020 are VERIFIED;
- Phase 8 is COMPLETE;
- Phase 9 remains pending until the aggregate run succeeds and the final metadata/tag are recorded.

## Compatibility

M260 introduces no C source, no new production object, and no linker-visible identifier. Therefore it introduces no new DEC C/VAX 31-character external-symbol risk.

Filename audit: `BUILD_M260.COM` and `doc/parity_audit_m260.md` do not collide by case with existing canonical paths.

## Completion criteria

M260 can be reconciled as complete only after a clean OpenVMS run shows:

- every invoked procedure succeeds;
- final `$STATUS` is `%X00000001` (or another documented odd success value, with `%X00000001` preferred by the authoritative checklist);
- the working tree remains clean;
- environment metadata and configured model are recorded; and
- the validated canonical commit is tagged after merge/post-merge validation.
