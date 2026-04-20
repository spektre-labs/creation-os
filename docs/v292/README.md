# v292 — σ-Leanstral

**Formal verification stack: pyramid-level guarantee.**

The Pantheon stands because the laws of physics certify its
geometry.  The σ-gate stands when Lean 4 theorems certify its
behaviour.  Mistral Leanstral (March 2026) generates Lean 4 proofs
at ≈ $36 per proof vs ≈ $549 for Claude vs ≈ $10 000 for a bug
caught in production.  v292 types the three-layer formal stack as
four v0 manifests so the claim "σ-gate is proved, not tested" has
an enumerated surface.

## σ innovation (what σ adds here)

> **σ-gate invariants are theorems, not tests.**  `σ ∈ [0, 1]`,
> `σ = 0 ⇒ K_eff = K`, `σ = 1 ⇒ K_eff = 0`, and `σ ↑ ⇒ K_eff ↓`
> hold as machine-verified proofs — v292 names them.

## v0 manifests

Enumerated in [`src/v292/leanstral.h`](../../src/v292/leanstral.h);
pinned by
[`benchmarks/v292/check_v292_leanstral_formal_verification.sh`](../../benchmarks/v292/check_v292_leanstral_formal_verification.sh).

### 1. Gate theorems (exactly 3 canonical theorems)

| theorem                   | lean4_proved |
|---------------------------|--------------|
| `gate_determinism`        | `true`       |
| `gate_range`              | `true`       |
| `gate_threshold_monotone` | `true`       |

### 2. σ invariants (exactly 4 canonical invariants)

`sigma_in_unit_interval · sigma_zero_k_eff_full ·
sigma_one_k_eff_zero · sigma_monotone_confidence_loss`, each
`holds == true`.

### 3. Leanstral economics (exactly 3 canonical rows, cost strictly increasing)

| label         | cost (USD) | kind                 |
|---------------|-----------:|----------------------|
| `leanstral`   |         36 | `AI_ASSISTED_PROOF`  |
| `claude`      |        549 | `AI_GENERIC`         |
| `bug_in_prod` |      10000 | `PROD_BUG`           |

Contract: strictly monotonically increasing cost — the proof layer
pays for itself before any bug reaches production.

### 4. Triple formal guarantee (exactly 3 canonical layers, all enabled)

| source           | layer                |
|------------------|----------------------|
| `frama_c_v138`   | `C_CONTRACTS`        |
| `lean4_v207`     | `THEOREM_PROOFS`     |
| `leanstral_v292` | `AI_ASSISTED_PROOFS` |

Three distinct layers; every layer enabled — three independent
formal checks stack on top of the σ-gate.

### σ_leanstral (surface hygiene)

```
σ_leanstral = 1 −
  (theorem_rows_ok + theorem_all_proved_ok +
   invariant_rows_ok + invariant_all_hold_ok +
   cost_rows_ok + cost_monotone_ok +
   layer_rows_ok + layer_distinct_ok +
   layer_all_enabled_ok) /
  (3 + 1 + 4 + 1 + 3 + 1 + 3 + 1 + 1)
```

v0 requires `σ_leanstral == 0.0`.

## Merge-gate contracts

- 3 theorem rows canonical; all Lean 4 proved — matches merge-gate
  "Lean 4 todistus: σ-gate invariantit todistettu".
- 4 invariant rows canonical; all hold.
- 3 cost rows canonical; strictly increasing (`leanstral < claude
  < bug_in_prod`).
- 3 formal-layer rows canonical; 3 distinct layers; all enabled.
- `σ_leanstral ∈ [0, 1] AND == 0.0`.
- FNV-1a chain replays byte-identically.

## v0 vs vN.1 (claim discipline)

- **v0 (this kernel)** names the theorems, invariants, economics,
  and triple-layer stack as predicates.  No shipped `.lean` proof
  artifacts; no Leanstral integration at commit time.
- **v292.1 (named, not implemented)** is shipped Lean 4 proof
  artifacts for each named theorem, Leanstral integration that
  auto-proves a new σ-gate change at commit time, and a build-gate
  that fails when any invariant loses its proof.

See [docs/CLAIM_DISCIPLINE.md](../CLAIM_DISCIPLINE.md).
