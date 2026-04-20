# v296 — σ-Antifragile

**Stronger from stress.**

Nassim Taleb: antifragile systems do not merely survive stress —
they get stronger from it. Entropy is fuel, not damage. v296
types four v0 manifests for that discipline: the stress-to-
adaptation loop lowers σ across cycles, σ volatility classifies
the regime, the σ-vaccine pre-exposes the system to controlled
noise, and a barbell allocation keeps the extremes while
rejecting the middle compromise.

## σ innovation (what σ adds here)

> **σ decreasing under increasing stress is the signature of
> antifragility.** v296 names the three variants of σ-volatility
> (unstable / stable / antifragile) and refuses to equate them.

## v0 manifests

Enumerated in [`src/v296/antifragile.h`](../../src/v296/antifragile.h);
pinned by
[`benchmarks/v296/check_v296_antifragile_stress_adaptation.sh`](../../benchmarks/v296/check_v296_antifragile_stress_adaptation.sh).

### 1. Stress → adaptation (exactly 3 canonical cycles)

| label     | stress_level | sigma_after |
|-----------|--------------|-------------|
| `cycle_1` | 1.0          | 0.50        |
| `cycle_2` | 2.0          | 0.35        |
| `cycle_3` | 3.0          | 0.25        |

Stress strictly monotonically increasing AND σ strictly
monotonically decreasing — antifragile, not merely robust.

### 2. σ-volatility tracking (exactly 3 canonical regimes)

| regime        | σ_mean | σ_std | trend       | classification |
|---------------|--------|-------|-------------|----------------|
| `unstable`    | 0.40   | 0.20  | NONE        | UNSTABLE       |
| `stable`      | 0.30   | 0.02  | NONE        | STABLE         |
| `antifragile` | 0.30   | 0.08  | DECREASING  | ANTIFRAGILE    |

`ANTIFRAGILE iff σ_std > std_stability=0.03 AND trend=DECREASING`;
`STABLE iff σ_std ≤ std_stability`;
`UNSTABLE iff σ_std > std_unstable=0.15 AND trend=NONE`.

### 3. Controlled exposure / σ-vaccine (exactly 3 canonical rows)

| label         | noise | is_vaccine | survived | because_trained |
|---------------|-------|------------|----------|-----------------|
| `dose_small`  | 0.10  | `true`     | `true`   | `false`         |
| `dose_medium` | 0.30  | `true`     | `true`   | `false`         |
| `real_attack` | 0.80  | `false`    | `true`   | `true`          |

Noise strictly monotonically increasing; all survived; exactly 2
vaccines; exactly 1 real attack survived because prior doses
trained the system.

### 4. Barbell strategy (exactly 3 canonical allocations)

| allocation           | share | τ    | kept    |
|----------------------|-------|------|---------|
| `safe_mode`          | 0.90  | 0.15 | `true`  |
| `experimental_mode`  | 0.10  | 0.70 | `true`  |
| `middle_compromise`  | 0.00  | 0.40 | `false` |

`share_safe + share_exp = 1.0 ± 1e-3`;
`share_middle = 0.0 ± 1e-3`; `τ_safe < τ_exp`; extremes kept,
middle rejected.

### σ_antifragile (surface hygiene)

```
σ_antifragile = 1 −
  (stress_rows_ok + stress_monotone_ok +
   vol_rows_ok + vol_classify_ok +
   vac_rows_ok + vac_noise_order_ok + vac_survived_ok +
   vac_vaccine_count_ok + vac_trained_ok +
   bb_rows_ok + bb_share_sum_ok + bb_middle_zero_ok +
   bb_tau_order_ok + bb_kept_polarity_ok) /
  (3 + 1 + 3 + 1 + 3 + 1 + 1 + 1 + 1 + 3 + 1 + 1 + 1 + 1)
```

v0 requires `σ_antifragile == 0.0`.

## Merge-gate contracts

- 3 stress cycles; stress ↑ AND σ ↓.
- 3 volatility regimes; 3 distinct classifications;
  ANTIFRAGILE iff σ_std > 0.03 AND trend=DECREASING.
- 3 vaccine rows; noise strictly increasing; all survived;
  exactly 2 vaccines; exactly 1 real attack survived because
  trained.
- 3 barbell rows; safe + exp = 1.0; middle = 0.0;
  τ_safe < τ_exp; extremes kept, middle rejected.
- `σ_antifragile ∈ [0, 1] AND == 0.0`.
- FNV-1a chain replays byte-identically.

## v0 vs vN.1 (claim discipline)

- **v0 (this kernel)** names the stress / volatility / vaccine /
  barbell contracts as predicates.
- **v296.1 (named, not implemented)** is live σ-volatility
  telemetry classifying the running system per minute, a
  scheduled σ-vaccine pipeline that injects controlled
  adversarial noise during low-traffic windows, and a barbell
  allocator that routes 10 % of production traffic through an
  experimental τ.

See [docs/CLAIM_DISCIPLINE.md](../CLAIM_DISCIPLINE.md).
