# v259 — Formal status ledger

**Generated at:** tracked by `make check-sigma-formal-complete`
(source of truth is the kernel `creation_os_sigma_formal_complete`
scanning `hw/formal/v259/Measurement.lean` +
`formal/lean/CreationOS/V133.lean` +
`hw/formal/v259/sigma_measurement.h.acsl` +
`hw/formal/v133/sigma_stack_contracts.acsl`).

This file exists so the public claim in `include/cos_version.h`
(`COS_FORMAL_PROOFS / COS_FORMAL_PROOFS_TOTAL`) never drifts away
from what the `.lean` / `.acsl` files actually prove. Per
[docs/CLAIM_DISCIPLINE.md](../CLAIM_DISCIPLINE.md), every number in
the banner must be backed by an artefact in this tree.

## Claim window

- **Banner:** `14/14` formal proof obligations discharged (Lean 4, zero `sorry`)
- **Toolchain:** core Lean 4 only (no Mathlib dependency); pinned via
  `hw/formal/v259/lean-toolchain` and `formal/lean/lean-toolchain`
  (`leanprover/lean4:stable`).
- **Verified by:** `cd hw/formal/v259 && lake build` and
  `cd formal/lean && lake build` exit 0 with zero warnings and zero
  `sorry`. Downstream greps in
  `benchmarks/sigma_pipeline/check_sigma_formal_complete.sh` and
  `benchmarks/sigma_pipeline/check_lean_t3_discharged.sh` assert that
  no `sorry` slipped back in.

## Six discharged gate theorems (T1 – T6)

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
`encode_injective`, `clampUnit_range`). They support the gate and C
bridge but are **not** part of the T1–T6 banner count.

## Eight stack lemmas (v133, `CreationOS.V133`)

Machine-checked abstract models in `formal/lean/CreationOS/V133.lean`
(sorry-free):

| Theorem | Role |
|---|---|
| `engram_stores_only_accept` | persistence guard accepts only `ACCEPT` |
| `abstain_does_not_propagate` | `ABSTAIN` routes to `Blocked` |
| `cascade_monotone` | cascade cost is monotone in the abstract level |
| `circuit_breaker_trips` | three high samples trip `CLOSED` → `OPEN` |
| `proconductor_overrides_all` | optional proconductor verdict overrides nodes |
| `kv_eviction_removes_highest_sigma` | interior list `≤` worst interior σ |
| `speculative_skip_safe` | low σ + `ACCEPT` ⇒ speculative skip bit |
| `staleness_increases_sigma` | staleness model never shrinks σ |

These lemmas are **abstractions** aligned with the inference stack;
they do not automatically refine to every production control path
without a separate proof obligation.

## Float ↔ Nat bridge

The Lean file models the gate over `Nat`, which is a true linear
order in core Lean 4 (no Mathlib, no `LinearOrder` class assumed).
The C-visible primitive `cos_sigma_measurement_gate(double, double,
double)` in `src/v259/sigma_measurement.h` operates on `double`.

The Nat ↔ IEEE-754 double bridge is:

* **Normal fragment** (finite non-NaN): the ordering on finite
  non-NaN doubles *is* a strict total order, and the Nat proofs
  transport unchanged modulo the standard `Float.toNat` / ULP
  indexing. This is the fragment every released v259 test sweep
  exercises.
* **NaN / ±∞ fragment**: handled explicitly in the C source by an
  early-out branch (`isnan(sigma) || isnan(tauA) || isnan(tauR)` →
  `ABSTAIN`) and annotated in `sigma_measurement.h.acsl` as a
  separate Frama-C Wp obligation.

Together, the core-Lean 4 Nat-level proofs plus the Frama-C Wp
obligations on the NaN branch form the full T1 – T6 discharge for
the Float instantiation.

## Frama-C: tier-1 Wp vs ACSL clause ledger

* **Tier-1 Wp (15 goals):** `cos_sigma_measurement_gate` and
  `cos_sigma_measurement_clamp` — reproduced by
  `scripts/v259/run_frama_c_wp.sh` when Frama-C + provers are installed.
* **ACSL clause ledger (requires + ensures lines, 30 total):**
  `sigma_measurement.h.acsl` + `hw/formal/v133/sigma_stack_contracts.acsl`,
  counted by `creation_os_sigma_formal_complete`. Companion stack
  contracts mirror KV / spike / speculative surfaces; **machine** Wp on
  those production translation units is optional R&D unless a maintainer
  extends the Wp scripts.

## Third source: runtime witness sweep

The Lean + ACSL layer is the *formal* leg. The third leg is the
runtime witness sampler in `src/v259/sigma_measurement.c`
(`cos_v259_roundtrip_exhaustive_check`,
`cos_v259_clamp_exhaustive_check`) — 10⁶-point LCG grids plus the
IEEE-754 special cross-product. Runtime evidence is strictly
weaker than the Wp proof over the full `cos_sigma_measurement_t`
domain; it is cited as a non-formal corroboration only.
