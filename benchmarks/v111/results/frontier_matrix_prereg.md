# v111.2 pre-registered adaptive σ matrix (test-split results)

- Lock file: `benchmarks/v111/PREREGISTRATION_ADAPTIVE.lock.json`
- Lock hash (adaptive_signals.py): `bb2ff2210be2e00c...`
- Split seed: `0xC05A1A2A`
- Bonferroni N = 12, α_fw = 0.00417
- n_boot = 2000

> **Pre-registered.** Every row is computed on the 50 % TEST split, held out from the router design AND from the conformal-τ calibration.  Results on this split replicate (or fail to replicate) the post-hoc finding.

### arc_challenge — n_cal=586 · n_test=586 · acc_test=0.4539 · entropy_AURCC=0.4805

| signal | AURCC_test ↓ | Δ vs entropy | CI95 | p | cov@acc≥0.95 | τ_conformal | Bonferroni |
|---|---:|---:|---|---:|---:|---:|:---:|
| `sigma_composite_max` | 0.4830 | +0.0022 | [-0.0326,+0.0362] | 0.9010 | 0.000 | 0.4631 |  |
| `sigma_composite_mean` | 0.4781 | -0.0025 | [-0.0259,+0.0212] | 0.8440 | 0.000 | 0.3266 |  |
| `sigma_task_adaptive` | 0.4736 | -0.0072 | [-0.0169,+0.0026] | 0.1450 | 0.000 | 0.1268 |  |

### arc_easy — n_cal=1188 · n_test=1188 · acc_test=0.7475 · entropy_AURCC=0.1778

| signal | AURCC_test ↓ | Δ vs entropy | CI95 | p | cov@acc≥0.95 | τ_conformal | Bonferroni |
|---|---:|---:|---|---:|---:|---:|:---:|
| `sigma_composite_max` | 0.1798 | +0.0021 | [-0.0141,+0.0179] | 0.7970 | 0.031 | 0.4442 |  |
| `sigma_composite_mean` | 0.1756 | -0.0022 | [-0.0129,+0.0085] | 0.6870 | 0.020 | 0.3322 |  |
| `sigma_task_adaptive` | 0.1778 | +0.0000 | [+0.0000,+0.0000] | 2.0000 | 0.009 | 0.2460 |  |

### truthfulqa_mc2 — n_cal=408 · n_test=409 · acc_test=0.4534 · entropy_AURCC=0.5576

| signal | AURCC_test ↓ | Δ vs entropy | CI95 | p | cov@acc≥0.95 | τ_conformal | Bonferroni |
|---|---:|---:|---|---:|---:|---:|:---:|
| `sigma_composite_max` | 0.4897 | -0.0681 | [-0.1068,-0.0313] | 0.0005 | 0.000 | 0.4153 | **yes** |
| `sigma_composite_mean` | 0.5025 | -0.0549 | [-0.0829,-0.0293] | 0.0005 | 0.000 | 0.2800 | **yes** |
| `sigma_task_adaptive` | 0.4897 | -0.0681 | [-0.1068,-0.0313] | 0.0005 | 0.000 | 0.4153 | **yes** |

### hellaswag — n_cal=373 · n_test=373 · acc_test=0.4692 · entropy_AURCC=0.4114

| signal | AURCC_test ↓ | Δ vs entropy | CI95 | p | cov@acc≥0.95 | τ_conformal | Bonferroni |
|---|---:|---:|---|---:|---:|---:|:---:|
| `sigma_composite_max` | 0.4209 | +0.0095 | [-0.0221,+0.0408] | 0.5620 | 0.008 | 0.2912 |  |
| `sigma_composite_mean` | 0.4163 | +0.0049 | [-0.0216,+0.0310] | 0.6990 | 0.008 | 0.2765 |  |
| `sigma_task_adaptive` | 0.4114 | +0.0000 | [+0.0000,+0.0000] | 2.0000 | 0.013 | 0.2875 |  |

### Pre-registration outcome
**H₀ REJECTED** on the test split: at least one (task, signal) pair passes Bonferroni at α_fw = 0.00417.  The adaptive σ router is promoted to a pre-registered claim on the families listed above.
