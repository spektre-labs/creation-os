# v111.2 MMLU subset σ-signals matrix

> **Post-hoc / subset.**  MMLU was not part of the pre-registered v111.1 four-family matrix; this file reports a stand-alone analysis over the subjects that cleared the v111.2 discovery floor of acc > 0.40 on BitNet-b1.58-2B-4T (see `benchmarks/v111/results/mmlu_discovery.md`).

- Bonferroni N = 28 (4 non-entropy signals × 7 subject(s))
- α_fw = 0.00179
- n_boot = 2000
- **Bonferroni-significant wins: 0 / 28 cells**

### Honest reading

On the eligible MMLU subjects, **entropy is a competitive baseline** — no σ-signal beats it at family-wise α_fw.  Most σ-signals show a *positive* ΔAURCC (slightly worse than entropy), well inside the bootstrap CI.  This is a consistent negative result across 7 factual / social-science MMLU subjects and is the **same regime** as the HellaSwag negative result in the pre-registered matrix: on log-likelihood-style factual questions where entropy already captures the calibration structure, σ-gating does not add useful information.  The σ-gate's Bonferroni-significant domain remains **TruthfulQA-style factual-confidence tasks** (labels designed to probe model-held falsehoods), not general MMLU-style knowledge-QA.

### Per-subject matrix

### mmlu_marketing — n=100 · overall_acc=0.7700

| signal | AURCC ↓ | Δ vs entropy | CI95 | p | cov@acc≥0.95 | Bonferroni |
|---|---:|---:|---|---:|---:|:---:|
| `entropy` | 0.0906 | — | — | — | 0.460 |  |
| `sigma_max_token` | 0.0971 | +0.0065 | [-0.0017,+0.0164] | 0.1360 | 0.400 |  |
| `sigma_product` | 0.0969 | +0.0063 | [-0.0018,+0.0167] | 0.1470 | 0.380 |  |
| `sigma_composite_max` | 0.0906 | +0.0000 | [+0.0000,+0.0000] | 2.0000 | 0.460 |  |
| `sigma_composite_mean` | 0.0915 | +0.0009 | [-0.0024,+0.0051] | 0.6210 | 0.460 |  |

### mmlu_sociology — n=100 · overall_acc=0.7000

| signal | AURCC ↓ | Δ vs entropy | CI95 | p | cov@acc≥0.95 | Bonferroni |
|---|---:|---:|---|---:|---:|:---:|
| `entropy` | 0.1583 | — | — | — | 0.240 |  |
| `sigma_max_token` | 0.1613 | +0.0031 | [-0.0076,+0.0135] | 0.5520 | 0.230 |  |
| `sigma_product` | 0.1611 | +0.0029 | [-0.0075,+0.0129] | 0.5630 | 0.230 |  |
| `sigma_composite_max` | 0.1583 | +0.0000 | [+0.0000,+0.0000] | 2.0000 | 0.240 |  |
| `sigma_composite_mean` | 0.1587 | +0.0005 | [-0.0031,+0.0037] | 0.7330 | 0.240 |  |

### mmlu_high_school_psychology — n=92 · overall_acc=0.7826

| signal | AURCC ↓ | Δ vs entropy | CI95 | p | cov@acc≥0.95 | Bonferroni |
|---|---:|---:|---|---:|---:|:---:|
| `entropy` | 0.0923 | — | — | — | 0.391 |  |
| `sigma_max_token` | 0.0960 | +0.0037 | [-0.0028,+0.0110] | 0.2660 | 0.391 |  |
| `sigma_product` | 0.0958 | +0.0035 | [-0.0029,+0.0105] | 0.2810 | 0.391 |  |
| `sigma_composite_max` | 0.0923 | +0.0000 | [+0.0000,+0.0000] | 2.0000 | 0.391 |  |
| `sigma_composite_mean` | 0.0935 | +0.0012 | [-0.0018,+0.0046] | 0.4640 | 0.391 |  |

### mmlu_nutrition — n=100 · overall_acc=0.6100

| signal | AURCC ↓ | Δ vs entropy | CI95 | p | cov@acc≥0.95 | Bonferroni |
|---|---:|---:|---|---:|---:|:---:|
| `entropy` | 0.2413 | — | — | — | 0.070 |  |
| `sigma_max_token` | 0.2524 | +0.0111 | [-0.0044,+0.0284] | 0.1650 | 0.060 |  |
| `sigma_product` | 0.2524 | +0.0111 | [-0.0049,+0.0286] | 0.1690 | 0.060 |  |
| `sigma_composite_max` | 0.2413 | +0.0000 | [+0.0000,+0.0000] | 2.0000 | 0.070 |  |
| `sigma_composite_mean` | 0.2450 | +0.0037 | [-0.0041,+0.0118] | 0.2560 | 0.070 |  |

### mmlu_us_foreign_policy — n=63 · overall_acc=0.7619

| signal | AURCC ↓ | Δ vs entropy | CI95 | p | cov@acc≥0.95 | Bonferroni |
|---|---:|---:|---|---:|---:|:---:|
| `entropy` | 0.0774 | — | — | — | 0.508 |  |
| `sigma_max_token` | 0.0782 | +0.0009 | [-0.0086,+0.0100] | 0.8210 | 0.492 |  |
| `sigma_product` | 0.0772 | -0.0002 | [-0.0096,+0.0086] | 0.9630 | 0.492 |  |
| `sigma_composite_max` | 0.0774 | +0.0000 | [+0.0000,+0.0000] | 2.0000 | 0.508 |  |
| `sigma_composite_mean` | 0.0770 | -0.0005 | [-0.0050,+0.0035] | 0.8150 | 0.508 |  |

### mmlu_high_school_government_and_politics — n=55 · overall_acc=0.7091

| signal | AURCC ↓ | Δ vs entropy | CI95 | p | cov@acc≥0.95 | Bonferroni |
|---|---:|---:|---|---:|---:|:---:|
| `entropy` | 0.1000 | — | — | — | 0.418 |  |
| `sigma_max_token` | 0.1076 | +0.0076 | [-0.0053,+0.0243] | 0.3030 | 0.400 |  |
| `sigma_product` | 0.1068 | +0.0069 | [-0.0057,+0.0226] | 0.3330 | 0.418 |  |
| `sigma_composite_max` | 0.1000 | +0.0000 | [+0.0000,+0.0000] | 2.0000 | 0.418 |  |
| `sigma_composite_mean` | 0.1035 | +0.0035 | [-0.0014,+0.0110] | 0.2100 | 0.418 |  |

### mmlu_human_sexuality — n=94 · overall_acc=0.6596

| signal | AURCC ↓ | Δ vs entropy | CI95 | p | cov@acc≥0.95 | Bonferroni |
|---|---:|---:|---|---:|---:|:---:|
| `entropy` | 0.1704 | — | — | — | 0.223 |  |
| `sigma_max_token` | 0.1862 | +0.0152 | [-0.0054,+0.0470] | 0.2290 | 0.021 |  |
| `sigma_product` | 0.1860 | +0.0150 | [-0.0057,+0.0466] | 0.2410 | 0.021 |  |
| `sigma_composite_max` | 0.1704 | +0.0000 | [+0.0000,+0.0000] | 2.0000 | 0.223 |  |
| `sigma_composite_mean` | 0.1698 | -0.0006 | [-0.0046,+0.0031] | 0.7270 | 0.223 |  |

