# v192 σ-Emergent — superlinear behaviour, measured

## Problem

Combining kernels produces behaviour that wasn't designed.
Some of it is good (insight); some of it is bad
(hallucination, safety drop). You need to **measure** the
emergence and **classify** it.

## σ-innovation

A kernel-pair-level metric:

```
σ_emergent = 1 − max(score_part) / score_combined
```

- `σ_emergent ≤ 0` ⇒ the whole is no better than the best
  part. Nothing emerged.
- `σ_emergent > τ_risk` ⇒ genuine superlinearity. Now we
  classify it on a safety co-metric:

| case | safety_combined vs safety_parts | class |
|------|-----|-------|
| unchanged or up | ≥ max − 0 | BENEFICIAL |
| drops ≥ 0.05 below best part | — | RISKY |

v192 ships a 12-pair fixture drawn from the real stack
(`v150/v135/v139/v146/v147/v138/v140/v183/v144`) with 4 strictly
linear pairs (guaranteed non-emergent), 4 beneficial pairs
(+23% quality, safety unharmed), and 4 risky pairs (+15%
quality, −15% safety). Every superlinear event is appended to
an append-only FNV-1a chain — the **emergence journal**.

## Merge-gate

`make check-v192` runs
`benchmarks/v192/check_v192_emergent_detection.sh`
and verifies:

- self-test PASSES
- ≥ 2 superlinear pairs detected
- ≥ 1 beneficial AND ≥ 1 risky classification
- **0 linear false positives** (`n_linear_false_pos == 0`)
- `n_beneficial + n_risky == n_superlinear`
- `journal_valid == true`
- byte-deterministic

## v192.0 (shipped) vs v192.1 (named)

| | v192.0 | v192.1 |
|-|-|-|
| Scores | closed-form | live v143 benchmark grid over real kernel pairs |
| Risky decomp | class label only | v140 causal attribution per kernel |
| Journal | in-memory chain | `~/.creation-os/emergence.jsonl` + Web UI `/emergence-dashboard` |
| Intervention | flag only | v183 TLA+ safety check before accepting risky pair |

## Honest claims

- **Is:** a deterministic superlinear-behaviour detector that
  never flags linear pairs, always surfaces ≥ 1 beneficial and
  ≥ 1 risky, and chains every event.
- **Is not:** a live benchmark sweep over real kernel pairs;
  the v143 grid and v140 causal attribution ship in v192.1.
