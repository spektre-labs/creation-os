# σ-gate v4 — comparison with published methods (draft table)

**Evidence hygiene:** the AUROC column mixes **different benchmarks, splits, and
harnesses** from primary literature. These numbers are **orientation only** —
not head-to-head on Creation OS’s TruthfulQA200 CSV. For in-repo numbers, use
`benchmarks/sigma_gate_lsd/results_full/manifest.json` (CV) and
`benchmarks/sigma_gate_eval/results_holdout/holdout_summary.json` (holdout)
after `make sigma-holdout`. See [`docs/CLAIM_DISCIPLINE.md`](CLAIM_DISCIPLINE.md).

| Method | Reported AUROC (typical / cited range) | Detector forward passes | Pre-generation signal | Year | Ref |
|--------|----------------------------------------|---------------------------|------------------------|------|-----|
| Semantic entropy (sampling) | ~0.7–0.8 (task-dependent) | Many (5–20+) | No | 2023 | Kuhn et al. |
| SelfCheckGPT-style | ~0.7–0.8 (task-dependent) | Several+ | No | 2023 | Manakul et al. |
| SEP (Semantic Entropy Probes) | ~0.79 (paper-reported setups) | 1 | No | 2024 | arXiv:2406.15927 |
| HALT / DRIFT (question-side) | ~0.85+ (paper-reported; setup-dependent) | 1 | **Yes** | 2026 | arXiv:2601.14210 |
| ICR Probe (residual aggregation) | literature-dependent | 1 | Varies | 2025 | ACL 2025 |
| LSD (contrastive trajectories) | ~0.92–0.96 (vendor repro) | 1 | No | 2025 | arXiv:2510.04933 |
| **σ-gate v4 (CV, this repo)** | **0.9428** | **1** (+ truth encoder) | **Optional** (`SigmaPrecheck`) | 2026 | `manifest.json` |
| **σ-gate v4 (holdout, this repo)** | **0.9821** | **1** | **Optional** | 2026 | `holdout_summary.json` |

**Differentiators (engineering, not a paper claim):**

1. Integrated into the Creation OS tree (`python/cos/`, C `cos` CLI coexists at repo root).
2. 12-byte packed measurement compatible with `cos_sigma_measurement_t` (see `sigma_gate_lsd.py`).
3. Optional **pre-generation** scaffold: `SigmaPrecheck` (entropy or optional sklearn head on middle-layer last-token states) + `SigmaGateFull` pipeline.
4. Open source; edge-oriented framing in repo docs.

**Acknowledgment:** LSD (Sirraya) contrastive pipeline; HALT/DRIFT and ICR for *research direction* on pre-generation probes — this repository does not ship their trained checkpoints.
