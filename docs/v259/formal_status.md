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

- **Banner:** `3/6` formal proofs discharged (Lean 4)
- **Theorem artefacts in the Lean file:** 14 (8 `gate*` lineage +
  3 `roundtrip*` + 1 `encode_injective` + 1 `clampUnit_range` + 1
  `rank_injective`)
- **Sorry-free theorems:** 10 (6 concrete, 4 abstract)
- **Theorems still `sorry`:** 4
  (`gate_totality`, `gate_monotone_in_sigma`,
  `gate_anti_monotone_in_tau`, `gate_boundary_tiebreak`)

`σ-formal-complete` treats a banner claim as honest iff
`header_proofs ≤ discharged_concrete + discharged_abstract`, capped
at `header_proofs_total`.  At the current count (6 + 4) the
banner could rise to `6/6`, but **we deliberately keep it at
`3/6`** because Lean 4's core `Float` does not admit a
`LinearOrder` instance (IEEE-754 NaN breaks antisymmetry), and the
Float-specific T3 / T4 / T5 obligations therefore need either
Frama-C Wp on the C source or a Mathlib-backed Float extension
before they can be counted as concrete Float discharges.

## Roadmap to 6/6

| Obligation | Concrete Float proof | Abstract LinearOrder lift | Evidence to flip banner |
|---|---|---|---|
| T1 purity | `gate_purity` (rfl) ✅ | — | already ≥1 |
| T2 totality | `gate_totality` (sorry) | `gateα_totality` ✅ | Frama-C Wp on `cos_sigma_measurement_gate` |
| T3 monotone σ | `gate_monotone_in_sigma` (sorry) | `gateα_monotone_in_sigma` ✅ | Frama-C Wp (NaN branch explicit in C) |
| T4 anti-monotone τ | `gate_anti_monotone_in_tau` (sorry) | `gateα_anti_monotone_in_tau` ✅ | Frama-C Wp |
| T5 boundary tiebreak | `gate_boundary_tiebreak` (sorry) | `gateα_boundary_tiebreak` ✅ | Frama-C Wp |
| T6 roundtrip/encode | `roundtrip_equiv` ✅, `encode_injective` ✅ | — | Frama-C Wp on `cos_sigma_measurement_roundtrip` memcpy lemma |

When a Frama-C / Alt-Ergo / Z3 toolchain is available:

```bash
frama-c \
  -cpp-extra-args="-Isrc/v259" \
  -wp -wp-prover alt-ergo,z3 -wp-rte -wp-timeout 30 \
  hw/formal/v259/sigma_measurement.h.acsl \
  src/v259/sigma_measurement.c
```

Each contract that reports `Proved` discharges one theorem.
`COS_FORMAL_PROOFS` can then be bumped to `4`, `5`, `6` in lockstep
with the proven count, and `make check-sigma-formal-complete` will
continue to succeed as long as the banner does not overclaim.

## Third source: runtime witness sweep

The Lean + ACSL layer is the *formal* leg.  The third leg is the
runtime witness sampler in `src/v259/sigma_measurement.c`
(`cos_v259_roundtrip_exhaustive_check`,
`cos_v259_clamp_exhaustive_check`) — 10⁶-point LCG grids plus the
IEEE-754 special cross-product.  Runtime evidence is strictly
weaker than the Wp proof over the full `cos_sigma_measurement_t`
domain; it is cited as a non-formal corroboration only.
