# v111.2 MMLU subset σ-signals matrix

> **Post-hoc / subset.**  MMLU was not part of the pre-registered v111.1 four-family matrix; this file reports a stand-alone analysis over whatever MMLU subject sidecars exist in `benchmarks/v111/results/`.

- Bonferroni N = 3 (3 signals × 1 subject(s))
- α_fw = 0.01667
- n_boot = 2000

### mmlu_abstract_algebra — n=100 · overall_acc=0.3300

| signal | AURCC ↓ | Δ vs entropy | CI95 | p | cov@acc≥0.95 | Bonferroni |
|---|---:|---:|---|---:|---:|:---:|
| `entropy` | 0.6512 | — | — | — | 0.000 |  |
| `sigma_max_token` | 0.6496 | -0.0011 | [-0.0466,+0.0425] | 0.9720 | 0.000 |  |
| `sigma_product` | 0.6478 | -0.0028 | [-0.0474,+0.0405] | 0.8970 | 0.000 |  |

