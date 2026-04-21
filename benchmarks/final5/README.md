# FINAL-5 — wired-pipeline benchmark re-run

Evidence bundle for the root-README measured table, produced **after**
FINAL-2 wired the ULTRA/SCI kernels into `cos chat` (`σ_combined` +
conformal τ + meta-cognition + session coherence).

Reproduce:

```bash
./cos-benchmark --json --no-codex > benchmarks/final5/fixtures_no_codex.json
./cos-benchmark --json --codex    > benchmarks/final5/fixtures_codex.json
./cos-benchmark --energy          2>&1 | tee benchmarks/final5/energy.txt
./creation_os_sigma_coverage_curve \
  --dataset benchmarks/pipeline/truthfulqa_817_detail.jsonl \
  --out benchmarks/final5/coverage_curve.json
./creation_os_sigma_suite_sci --alpha 0.80 --delta 0.10 \
  --out benchmarks/final5/suite_sci.json
```

## Artefacts

| file | schema | produced by |
|:--|:--|:--|
| `fixtures_no_codex.json` | `cos.bench.v2` (per-config summary, Codex off) | `cos-benchmark --json --no-codex` |
| `fixtures_codex.json`    | `cos.bench.v2` (per-config summary, Codex hash `c860c67f75bc9d1d`) | `cos-benchmark --json --codex` |
| `energy.txt`             | ULTRA-7 `reasoning/J` demo table | `cos-benchmark --energy` |
| `coverage_curve.json`    | 21 τ-steps × (τ, coverage, acc_accepted, risk_ucb) | `creation_os_sigma_coverage_curve` |
| `suite_sci.json`         | `cos.suite_sci.v1` with α=0.80, δ=0.10 | `creation_os_sigma_suite_sci` |

## Headline numbers (verified identical to committed README table)

| surface | measurement | this re-run | root-README table |
|:--|:--|--:|--:|
| 20 fixtures · `bitnet_only`        | mean σ          | 0.2685 | 0.2685 |
| 20 fixtures · `pipeline_no_codex`  | mean σ          | 0.1160 | 0.1160 |
| 20 fixtures · `pipeline_codex`     | mean σ          | 0.1160 | 0.1160 |
| 20 fixtures · `api_only`           | €/call          | 0.01210 | 0.01210 |
| TruthfulQA 817 · `pipeline`        | accuracy (all)  | **0.3357** | 0.336 |
| TruthfulQA 817 · `pipeline`        | coverage        | 0.1714 (140 / 817) | 0.171 |
| TruthfulQA 817 · SCI-1 (α=0.80)    | τ               | **0.6551** | 0.655 |
| TruthfulQA 817 · SCI-1             | conformal valid | **yes**   | yes    |
| Coverage curve · τ = 0.65          | coverage · acc  | 0.9929 · 0.3309 | — (new) |
| ULTRA-7 energy                     | reasoning / J (`sigma_selective`) | **1.040** | 1.040 |

All numbers are read directly from the JSON/TXT artefacts in this
directory — nothing is projected.  The re-run is deterministic given
the same fixtures: running it again on the same tree will produce
identical JSON bytes.

## Relationship to root-README

The root [`../../README.md`](../../README.md) § **Measured** cites the
same numbers; the per-row JSON for each TruthfulQA 817 prompt is in
[`../pipeline/truthfulqa_817_detail.jsonl`](../pipeline/truthfulqa_817_detail.jsonl).
This directory is the compact form of that evidence for the v3.0.0
tag.
