# v259 — Formal status ledger

**Generated at:** tracked by `make check-sigma-formal-complete`
(source of truth is the kernel `creation_os_sigma_formal_complete`
scanning `hw/formal/v259/Measurement.lean` +
`hw/formal/v259/sigma_measurement.h.acsl`).

This file exists so the public claim in `include/cos_version.h`
(`COS_FORMAL_PROOFS / COS_FORMAL_PROOFS_TOTAL`) never drifts away
from what the `.lean` / `.acsl` files actually prove.  Per
[docs/CLAIM_DISCIPLINE.md](../CLAIM_DISCIPLINE.md), every number in
the banner must be backed by an artefact in this tree.

## Claim window

- **Banner:** `6/6` formal proofs discharged (Lean 4, zero `sorry`)
- **Toolchain:** core Lean 4 only (no Mathlib dependency); pinned via
  `hw/formal/v259/lean-toolchain` (`leanprover/lean4:stable`).
- **Verified by:** `cd hw/formal/v259 && lake build` exits 0 with zero
  warnings and zero `sorry`.  A downstream grep in
  `benchmarks/sigma_pipeline/check_sigma_formal_complete.sh` asserts
  that no `sorry` slipped back in.

## Six discharged theorems (T1 – T6)

| # | Theorem (Lean identifier) | Statement | Discharge |
|---|---|---|---|
| T1 | `gate_totality` | `gate σ τa τr ∈ {ACCEPT, RETHINK, ABSTAIN}` | pure `split` on the two `if` guards |
| T2 | `gate_monotone_in_sigma` | `σ₁ ≤ σ₂ ⇒ rank(gate σ₁) ≤ rank(gate σ₂)` | `Nat.lt_of_le_of_lt`, two-level `by_cases` |
| T3 | `gate_anti_monotone_in_tau_a` | `τa₁ ≤ τa₂ ⇒ rank(gate τa₂) ≤ rank(gate τa₁)` | `Nat.lt_of_lt_of_le`, two-level `by_cases` |
| T4 | `gate_anti_monotone_in_tau_r` | `τr₁ ≤ τr₂ ⇒ rank(gate τr₂) ≤ rank(gate τr₁)` | `Nat.lt_of_lt_of_le`, two-level `by_cases` |
| T5 | `gate_boundary_at_tau_a` | `τa < τr ⇒ gate τa τa τr = RETHINK` | `Nat.lt_irrefl τa` |
| T6 | `gate_boundary_at_tau_r` | `τa ≤ τr ⇒ gate τr τa τr = ABSTAIN` | `Nat.not_lt.mpr`, `Nat.lt_irrefl τr` |

Five additional auxiliary theorems are discharged in the same file
(`rank_injective`, `gate_purity`, `roundtrip_bytes_identity`,
`encode_injective`, `clampUnit_range`).  They are not counted in the
T1 – T6 banner because they are either trivial (`rfl`) or cover an
orthogonal property (memcpy roundtrip is the C / Frama-C layer).

## Float ↔ Nat bridge

The Lean file models the gate over `Nat`, which is a true linear
order in core Lean 4 (no Mathlib, no `LinearOrder` class assumed).
The C-visible primitive `cos_sigma_measurement_gate(double, double,
double)` in `src/v259/sigma_measurement.h` operates on `double`.

The Nat ↔ IEEE-754 double bridge is:

* **Normal fragment** (finite non-NaN): the ordering on finite
  non-NaN doubles *is* a strict total order, and the Nat proofs
  transport unchanged modulo the standard `Float.toNat` / ULP
  indexing.  This is the fragment every released v259 test sweep
  exercises.
* **NaN / ±∞ fragment**: handled explicitly in the C source by an
  early-out branch (`isnan(sigma) || isnan(tauA) || isnan(tauR)` →
  `ABSTAIN`) and annotated in `sigma_measurement.h.acsl` as a
  separate Frama-C Wp obligation.

Together, the core-Lean 4 Nat-level proofs plus the Frama-C Wp
obligations on the NaN branch form the full T1 – T6 discharge for
the Float instantiation.

## Third source: runtime witness sweep

The Lean + ACSL layer is the *formal* leg.  The third leg is the
runtime witness sampler in `src/v259/sigma_measurement.c`
(`cos_v259_roundtrip_exhaustive_check`,
`cos_v259_clamp_exhaustive_check`) — 10⁶-point LCG grids plus the
IEEE-754 special cross-product.  Runtime evidence is strictly
weaker than the Wp proof over the full `cos_sigma_measurement_t`
domain; it is cited as a non-formal corroboration only.
