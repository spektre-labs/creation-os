# v177 σ-Compress — σ-guided model compression

## Problem

BitNet-2B is already compact, but a deployable kernel for edge
hardware (v165) wants it smaller *without* destroying σ
calibration — the quality signal the rest of Creation OS
depends on.

## σ-innovation

Three σ-aware passes over a synthetic 16×64 layer stack:

1. **σ-aware pruning.** Drop every neuron whose `σ_impact <
   prune_tau (0.05)`: removing them changes calibration by
   less than the measurement noise.  Each layer's `σ_layer`
   is nudged by the *proportional* lost σ-impact, so the
   change is bounded by a few percent.
2. **σ-aware mixed precision.**
   - `σ_layer ≤ 0.15` → INT8 (critical)
   - `σ_layer ≤ 0.40` → INT4
   - otherwise        → INT2
3. **σ-layer merging.** Collapse consecutive layers with
   `|Δσ_layer| ≤ merge_tau (0.03)` and identical precision.

Under the baked seed the kernel produces:

| metric              | before | after | Δ |
|---------------------|-------:|------:|-----:|
| params              | 1 024  | 204   | −80.1 % |
| bytes               | 1 024  |  ≈88  | ≈−91 % |
| layers              | 16     | 10    | 6 merged |
| σ-calibration drift | —      | —     | **0.72 %** (budget 5 %) |
| precision split     | —      | 3 × INT8, 3 × INT4, 4 × INT2 |

## Merge-gate

`make check-v177` runs
`benchmarks/v177/check_v177_compress_sigma_prune.sh` and
verifies:

- self-test
- `params_after < 0.70 · params_before` (≥ 30 % reduction)
- mixed precision uses all three bands
- ≥ 1 layer merged; `layers_after + merged == layers_before`
- `σ_drift_pct ≤ drift_budget_pct (5.0 %)`
- `bytes_after < bytes_before`
- JSON byte-identical across runs

## v177.0 (shipped) vs v177.1 (named)

| | v177.0 | v177.1 |
|-|-|-|
| Target | synthetic stack | real BitNet-2B |
| Output | JSON report | `models/v177/bitnet_1b_sigma_pruned.gguf` |
| Quant | precision flag | actual activation INT4/INT2 pass |
| Merge | flag-only | weight/bias fusion |

## Honest claims

- **Is:** an offline kernel proving the three σ-guided
  compression primitives produce a ≥ 30 %  parameter drop
  inside a ≤ 5 % σ-calibration budget.
- **Is not:** a quantiser.  No real weights move in v0; the
  contract is the report numbers and the drift bound.
