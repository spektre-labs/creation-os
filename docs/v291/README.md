# v291 — σ-Parthenon

**Calibration and optical correction: σ tuned to context.**

The Parthenon's columns bulge slightly in the middle (entasis) so
they do not look concave to a human eye.  The σ-gate needs the same
optical correction: σ = 0.30 means something different in medicine,
code, and creative writing; a raw σ value can be systematically
over- or under-confident; the extremes σ = 0.00 and σ = 1.00 are
unreliable.  v291 types the correction discipline as four v0
manifests.

## σ innovation (what σ adds here)

> **σ is the signal; v291 names the four ways that signal needs
> interpretation:** context calibration (what σ means per domain),
> perception correction (what σ means to a human), bias correction
> (what σ means after systematic error is removed), and entasis
> (never fully certain, never fully uncertain).

## v0 manifests

Enumerated in [`src/v291/parthenon.h`](../../src/v291/parthenon.h);
pinned by
[`benchmarks/v291/check_v291_parthenon_calibration.sh`](../../benchmarks/v291/check_v291_parthenon_calibration.sh).

### 1. Context calibration (exactly 3 canonical domains, shared σ = 0.30)

| domain     | verdict    |
|------------|------------|
| `medical`  | `ABSTAIN`  |
| `code`     | `CAUTIOUS` |
| `creative` | `SAFE`     |

Contract: 3 distinct verdicts; every row uses the shared
`sigma_sample = 0.30` — v291 demonstrates σ = 0.30 is read three
ways by three domains.

### 2. Perception correction (exactly 3 canonical σ values)

| σ     | ratio_denominator | explanation_present |
|-------|-------------------|---------------------|
| 0.05  | 20                | `true`              |
| 0.15  | 7                 | `true`              |
| 0.50  | 2                 | `true`              |

Contract: `ratio_denominator == round(1 / σ)` on every row.

### 3. Bias correction (exactly 3 canonical bias types, budget 0.02)

| bias_type         | σ_raw | offset | σ_corrected | residual_bias |
|-------------------|-------|--------|-------------|---------------|
| `overconfident`   | 0.20  | +0.10  | 0.30        | 0.00          |
| `underconfident`  | 0.60  | −0.10  | 0.50        | 0.00          |
| `calibrated`      | 0.40  |  0.00  | 0.40        | 0.00          |

Contract: `σ_corrected == σ_raw + offset`; `overconfident` offset
> 0; `underconfident` offset < 0; `calibrated` offset = 0;
`residual_bias ≤ bias_budget (0.02)` on every row — matches
the merge-gate bound "bias < 0.02 kaikissa domaineissa".

### 4. Entasis clamp (exactly 3 canonical inputs, bounds [0.02, 0.98])

`σ_in = 0.005 → σ_clamped = 0.02`;
`σ_in = 0.995 → σ_clamped = 0.98`;
`σ_in = 0.500 → σ_clamped = 0.50`.

Contract: `σ_clamped == clamp(σ_in, lower_bound, upper_bound)` on
every row — the σ-gate is never perfectly certain, never perfectly
uncertain.

### σ_parthenon (surface hygiene)

```
σ_parthenon = 1 −
  (calibration_rows_ok + calibration_verdict_distinct_ok +
   calibration_sample_shared_ok + perception_rows_ok +
   perception_ratio_ok + perception_explanation_ok +
   bias_rows_ok + bias_formula_ok + bias_polarity_ok +
   bias_budget_ok + entasis_rows_ok +
   entasis_clamp_formula_ok) /
  (3 + 1 + 1 + 3 + 1 + 1 + 3 + 1 + 1 + 1 + 3 + 1)
```

v0 requires `σ_parthenon == 0.0`.

## Merge-gate contracts

- 3 calibration rows canonical; 3 distinct verdicts; shared σ.
- 3 perception rows; ratio matches `round(1/σ)`; explanations
  present.
- 3 bias rows; `corrected = raw + offset`; polarity signs match
  labels; `residual_bias ≤ 0.02`.
- 3 entasis rows; clamp formula holds.
- `σ_parthenon ∈ [0, 1] AND == 0.0`.
- FNV-1a chain replays byte-identically.

## v0 vs vN.1 (claim discipline)

- **v0 (this kernel)** types the four calibration surfaces as
  predicates.  No live bias-from-telemetry loop, no production
  entasis clamp on outbound σ.
- **v291.1 (named, not implemented)** is a live calibration loop
  that measures raw-vs-actual error rates per domain and computes
  `bias_offset` from telemetry, a production entasis clamp on
  every exposed σ, and a UI that shows the human-readable ratio
  next to every σ.

See [docs/CLAIM_DISCIPLINE.md](../CLAIM_DISCIPLINE.md).
