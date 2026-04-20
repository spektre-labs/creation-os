# v259.1 — formal scaffolding for `sigma_measurement_t`

**Status:** proof obligations written down; no theorem discharged yet.

This directory holds the machine-checkable contract for the 12-byte
Creation OS σ-measurement primitive defined in
[`src/v259/sigma_measurement.h`](../../../src/v259/sigma_measurement.h).

## Files

| file | language | what it states |
|---|---|---|
| `sigma_measurement.h.acsl` | Frama-C ACSL | Layout, purity, roundtrip, τ-monotonicity, benchmark budget. |
| `Measurement.lean`         | Lean 4        | Gate totality, σ-monotone, τ-anti-monotone, boundary tiebreak, roundtrip equivalence. |

## Honesty note (CLAIM_DISCIPLINE)

Per [docs/CLAIM_DISCIPLINE.md](../../../docs/CLAIM_DISCIPLINE.md):

* Every `//@ requires / ensures` clause in the ACSL file is tagged
  `PROOF: PENDING` in the preceding comment.
* Every Lean 4 theorem body is either `rfl` (trivially discharged)
  or ends in `sorry` (PENDING).
* The only mechanical enforcement of the v259 contract today is the
  C self-test in `src/v259/sigma_measurement.c` and the shell assertions
  in `benchmarks/v259/check_v259_sigma_measurement_primitive.sh`.

The purpose of this directory is to **freeze the contract in a
verifiable form** so that any future regression becomes a proof
obligation, not a prose argument.

## Future CI integration (PENDING)

* `make formal-v259` runs Frama-C Wp on the ACSL file and Lean 4 on
  `Measurement.lean`, fails hard on any unresolved goal.
* This target will be added to `merge-gate` once both toolchains are
  reproducibly available in the build sandbox.  Until then it is
  documented, not invoked.
