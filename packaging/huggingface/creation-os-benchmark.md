---
license: cc-by-4.0
language:
- en
tags:
- sigma
- calibration
- abstention
- uncertainty-quantification
- benchmark
- creation-os
pretty_name: Creation OS σ-Benchmark
size_categories:
- 1K<n<10K
task_categories:
- text-classification
- question-answering
---

# Creation OS σ-Benchmark (v143)

Five σ-native categories — **σ-Calibration** (ECE), **σ-Abstention**
(coverage @ 95 % accuracy), **σ-Swarm-Routing** (argmin σ across K
specialists), **σ-Learning** (Δ-accuracy with no-forgetting
hold-out), **σ-Adversarial** (detection @ FPR ≤ 5 %) — emitted by
the v143 kernel to a single canonical JSON at
`benchmarks/v143/creation_os_benchmark.json`.

Every row in the artefact carries a `"tier"` field: `synthetic`
(tier-0, deterministic SplitMix64 + Box-Muller), `archived` (tier-1,
post-hoc archived σ-traces from real runs), or `live` (tier-2, live
model + live data). The **v143.0** release is all `tier: synthetic`
— the numbers are reproducible byte-for-byte from a fixed seed but
do not imply a measurement on a live LLM.

## Reproduction

```bash
git clone https://github.com/spektre-labs/creation-os
cd creation-os
make check-v143
cat benchmarks/v143/creation_os_benchmark.json
```

## Expected floors (v143.0 synthetic tier)

| Category | Metric | Floor |
|---|---|---|
| σ-Calibration | ECE | < 0.15 |
| σ-Abstention | coverage @ acc ≥ 95 % | > 0.30 |
| σ-Swarm-Routing | argmin-σ match rate | > 0.80 |
| σ-Swarm-Routing | σ-spread across specialists | > 0.30 |
| σ-Learning | Δ-accuracy | > 0.00 |
| σ-Learning | \|drift\| on hold-out | < 0.10 |
| σ-Adversarial | detection @ FPR ≤ 5 % | > 0.70 |

## Provenance

- Source: [`spektre-labs/creation-os`](https://github.com/spektre-labs/creation-os)
- Kernel: [`src/v143/`](https://github.com/spektre-labs/creation-os/tree/main/src/v143)
- Merge-gate: `make check-v143`
- License: CC-BY-4.0 for data, SCSL-1.0 / AGPL-3.0-only for code.

## Citation

```bibtex
@misc{creation_os_benchmark_v143,
  title  = {Creation OS {{σ-Benchmark}} v143},
  author = {Rainio, Lauri Elias and {Spektre Labs}},
  year   = {2026},
  url    = {https://github.com/spektre-labs/creation-os}
}
```
