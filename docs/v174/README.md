# v174 σ-Flywheel — self-evolving synthetic-data pipeline

## Problem

A naive data flywheel (Proposer → Solver → Trainer) collapses
onto itself: whatever the Solver writes becomes training data,
which the next Solver imitates, which the next Trainer learns,
until the model is confidently wrong on an ever-narrower
distribution.

## σ-innovation

v174 makes **σ the data filter**. Every generated sample is
classified by the Solver's σ:

| σ band              | class  | usage                                  |
|---------------------|--------|----------------------------------------|
| `σ < τ_confident` (0.25) | **chosen** | positive example for DPO        |
| `τ_c ≤ σ ≤ τ_u`     | **skip**   | uninformative — never trained on  |
| `σ > τ_uncertain` (0.60) | **pair**   | negative; big model supplies chosen |

And three anti-model-collapse guards fire if the cycle is
degenerating, **aborting the flywheel** with a reason:

1. Answer-cluster entropy `H < h_min = 1.20`
2. σ-variance across samples `< var_min = 0.010`
3. Benchmark score `< baseline − slack` (0.05)

Under the baked seed (`0x174F1A1BEEF`) one cycle produces
50 samples over 5 clusters with `chosen/pair/skip = 19/22/9`,
`H = 1.59`, `σ-variance = 0.099`, score 0.69 — well clear of
every guard.

## Merge-gate

`make check-v174` runs
`benchmarks/v174/check_v174_flywheel_one_cycle.sh` and
verifies:

- self-test
- one cycle: `n_chosen + n_pair + n_skip == 50`, each class
  has ≥ 1 sample
- all 5 clusters hit (`distinct_clusters == 5`)
- entropy > `h_min`, σ-variance > `var_min`, no collapse
- DPO NDJSON contains only `chosen` and `pair` records;
  every `pair` has `σ_chosen < σ_rejected`; SKIPs never leak
- regression guard fires with baseline = 0.99 and the reason
  mentions "baseline" / "score"
- JSON + DPO byte-identical across runs

## v174.0 (shipped) vs v174.1 (named)

| | v174.0 | v174.1 |
|-|-|-|
| Proposer | deterministic fixture | real v151 code-agent |
| Big-model corrector | σ stub | real v114 swarm |
| Trainer | no-op | v125 DPO + v124 hot-swap |
| Benchmark | closed-form score | real v143 smoke |

## Honest claims

- **Is:** a deterministic, offline σ-filtered flywheel
  demonstrating chosen/pair/skip partitioning, 5-cluster
  diversity, and three working anti-collapse guards.
- **Is not:** a training pipeline. No weights move in v0;
  the contract is the data shape and the collapse-trigger
  semantics. Real training is v174.1.
