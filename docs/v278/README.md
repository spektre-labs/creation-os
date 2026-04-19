# v278 — σ-Recursive-Self-Improve (`docs/v278/`)

σ itself is a manifest that improves over time.  v278
types four loops: calibration feedback, σ-gate
architecture search, per-domain threshold adaptation,
and a Gödel-aware call-out when the gate cannot see its
own blind spot.

> Canonical Creation OS remote:
> [spektre-labs/creation-os](https://github.com/spektre-labs/creation-os) —
> see [../CANONICAL_GIT_REPOSITORY.md](../CANONICAL_GIT_REPOSITORY.md).

## σ-innovation

### Calibration feedback (exactly 4 epochs)

Contract: strictly decreasing; last epoch
`sigma_calibration_err ≤ 0.05`.

| epoch | σ_calibration_err |
|------:|------------------:|
|  0    | 0.22              |
|  1    | 0.15              |
|  2    | 0.09              |
|  3    | 0.04              |

### Architecture search (exactly 3 configurations)

Contract: `chosen == argmax(aurcc)`; exactly one
chosen; ≥ 2 distinct aurcc values (a regression that
ties everything fails).

| n_channels | aurcc | chosen |
|-----------:|------:|:------:|
|  6         | 0.78  | —      |
|  8         | 0.91  | ✅     |
| 12         | 0.84  | —      |

### Threshold adaptation (exactly 3 domains)

Contract: `code.tau == 0.20`, `creative.tau == 0.50`,
`medical.tau == 0.15`; all in (0, 1); ≥ 2 distinct
values.

| domain     | τ     |
|------------|------:|
| `code`     | 0.20  |
| `creative` | 0.50  |
| `medical`  | 0.15  |

### Gödel awareness (exactly 3 fixtures, τ_goedel = 0.40)

Rule: `σ_goedel ≤ τ_goedel → SELF_CONFIDENT` else
`CALL_PROCONDUCTOR` (there is a blind spot; delegate
to an external verifier).  Both branches fire.

| claim_id | σ_goedel | action              |
|---------:|---------:|:-------------------:|
|  0       | 0.12     | SELF_CONFIDENT      |
|  1       | 0.37     | SELF_CONFIDENT      |
|  2       | 0.64     | CALL_PROCONDUCTOR   |

### σ_rsi

```
σ_rsi = 1 − (calibration_rows_ok +
             calibration_monotone_ok +
             calibration_anchor_ok +
             arch_rows_ok + arch_chosen_ok +
             arch_distinct_ok +
             threshold_rows_ok +
             threshold_canonical_ok +
             threshold_distinct_ok +
             goedel_rows_ok + goedel_both_ok) /
            (4 + 1 + 1 + 3 + 1 + 1 + 3 + 1 + 1 + 3 + 1)
```

v0 requires `σ_rsi == 0.0`.

## Merge-gate contract

`bash benchmarks/v278/check_v278_recursive_self_improve.sh`

- self-test PASSES
- 4 calibration epochs strictly decreasing; last ≤
  0.05
- 3 arch configs (6/8/12); `chosen == argmax(aurcc)`;
  exactly one; ≥ 2 distinct aurcc
- 3 thresholds canonical; each τ ∈ (0, 1); ≥ 2
  distinct τ
- 3 Gödel rows; action matches σ_goedel vs τ_goedel =
  0.40; both branches fire
- `σ_rsi ∈ [0, 1]` AND `σ_rsi == 0.0`
- chain valid + byte-deterministic

## v0 vs v1 split

- **v0 (this tree)** — typed calibration / arch /
  threshold / Gödel manifest with FNV-1a chain.
- **v278.1 (named, not implemented)** — live
  calibration tuning on a feedback stream, CMA-ES
  architecture search over σ-gate channel counts,
  per-domain τ adaptation fed by production σ
  histograms, and a production proconductor call-out
  pipeline when `σ_goedel > τ_goedel`.

## Honest claims

- **Is:** a typed, falsifiable σ-RSI manifest where
  calibration monotonicity, architecture argmax,
  canonical threshold layout, and the Gödel-aware
  branching are merge-gate predicates.
- **Is not:** a self-improving system.  v278.1 is
  where the manifest meets a live feedback loop; the
  v0 numbers in the manifest are fixture data, not
  measurements.

---

*Spektre Labs · Creation OS · 2026*
