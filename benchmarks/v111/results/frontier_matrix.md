# v111 frontier parity matrix

- gating signals (6): entropy, sigma_mean, sigma_max_token, sigma_product, sigma_tail_mass, sigma_n_effective
- task families (4): arc_challenge, arc_easy, truthfulqa_mc2, hellaswag
- Bonferroni N = 24, α = 0.05 / 24 = 0.00208
- n_boot = 500

Baseline = `entropy` (channel 0 of σ profile). AURCC lower = better selective-prediction calibration. `Bonferroni` column marks signals that beat the baseline below the family-wise α.

### arc_challenge — n=1172 · acc=0.4676

| signal | AURCC ↓ | ΔAURCC vs entropy | CI95 | p | cov@acc≥0.90 | cov@acc≥0.95 | Bonferroni |
|---|---:|---:|---|---:|---:|---:|:---:|
| `entropy` | 0.4672 | +0.0000 | [+0.0000,+0.0000] | 2.0000 | 0.002 | 0.002 |  |
| `sigma_mean` | 0.4712 | +0.0041 | [-0.0027,+0.0110] | 0.2120 | 0.003 | 0.003 |  |
| `sigma_max_token` | 0.4572 | -0.0099 | [-0.0317,+0.0111] | 0.3920 | 0.001 | 0.001 |  |
| `sigma_product` | 0.4585 | -0.0087 | [-0.0147,-0.0026] | 0.0080 | 0.003 | 0.003 |  |
| `sigma_tail_mass` | 0.4521 | -0.0155 | [-0.0336,+0.0003] | 0.0640 | 0.001 | 0.001 |  |
| `sigma_n_effective` | 0.4591 | -0.0087 | [-0.0178,-0.0000] | 0.0480 | 0.000 | 0.000 |  |


### arc_easy — n=2376 · acc=0.7551

| signal | AURCC ↓ | ΔAURCC vs entropy | CI95 | p | cov@acc≥0.90 | cov@acc≥0.95 | Bonferroni |
|---|---:|---:|---|---:|---:|---:|:---:|
| `entropy` | 0.1710 | +0.0000 | [+0.0000,+0.0000] | 2.0000 | 0.187 | 0.007 |  |
| `sigma_mean` | 0.1727 | +0.0017 | [-0.0016,+0.0048] | 0.3520 | 0.186 | 0.007 |  |
| `sigma_max_token` | 0.1691 | -0.0019 | [-0.0123,+0.0095] | 0.7280 | 0.096 | 0.028 |  |
| `sigma_product` | 0.1712 | +0.0002 | [-0.0024,+0.0029] | 0.9200 | 0.184 | 0.010 |  |
| `sigma_tail_mass` | 0.1807 | +0.0095 | [+0.0024,+0.0163] | 0.0120 | 0.060 | 0.003 |  |
| `sigma_n_effective` | 0.1721 | +0.0009 | [-0.0033,+0.0051] | 0.6800 | 0.174 | 0.019 |  |


### truthfulqa_mc2 — n=817 · acc=0.4638

| signal | AURCC ↓ | ΔAURCC vs entropy | CI95 | p | cov@acc≥0.90 | cov@acc≥0.95 | Bonferroni |
|---|---:|---:|---|---:|---:|---:|:---:|
| `entropy` | 0.5528 | +0.0000 | [+0.0000,+0.0000] | 2.0000 | 0.000 | 0.000 |  |
| `sigma_mean` | 0.5617 | +0.0090 | [+0.0017,+0.0162] | 0.0200 | 0.000 | 0.000 |  |
| `sigma_max_token` | 0.5080 | -0.0442 | [-0.0709,-0.0199] | 0.0020 | 0.001 | 0.001 | **yes** |
| `sigma_product` | 0.5411 | -0.0112 | [-0.0183,-0.0042] | 0.0040 | 0.000 | 0.000 |  |
| `sigma_tail_mass` | 0.5267 | -0.0251 | [-0.0418,-0.0085] | 0.0040 | 0.000 | 0.000 |  |
| `sigma_n_effective` | 0.5316 | -0.0204 | [-0.0305,-0.0105] | 0.0020 | 0.001 | 0.001 | **yes** |


### hellaswag — n=746 · acc=0.4853

| signal | AURCC ↓ | ΔAURCC vs entropy | CI95 | p | cov@acc≥0.90 | cov@acc≥0.95 | Bonferroni |
|---|---:|---:|---|---:|---:|---:|:---:|
| `entropy` | 0.3796 | +0.0000 | [+0.0000,+0.0000] | 2.0000 | 0.034 | 0.028 |  |
| `sigma_mean` | 0.3877 | +0.0080 | [+0.0006,+0.0152] | 0.0240 | 0.031 | 0.019 |  |
| `sigma_max_token` | 0.4288 | +0.0501 | [+0.0183,+0.0783] | 0.0020 | 0.007 | 0.007 | HURTS |
| `sigma_product` | 0.3780 | -0.0013 | [-0.0089,+0.0057] | 0.6840 | 0.034 | 0.029 |  |
| `sigma_tail_mass` | 0.3785 | -0.0008 | [-0.0154,+0.0133] | 0.9040 | 0.021 | 0.003 |  |
| `sigma_n_effective` | 0.3866 | +0.0078 | [-0.0104,+0.0244] | 0.3680 | 0.015 | 0.009 |  |


### Cross-task best signal (Bonferroni wins count)

| signal | Bonferroni wins | mean ΔAURCC | tasks beaten |
|---|---:|---:|---|
| `sigma_mean` | 0 | +0.0057 | — |
| `sigma_max_token` | 1 | -0.0015 | truthfulqa_mc2 |
| `sigma_product` | 0 | -0.0052 | — |
| `sigma_tail_mass` | 0 | -0.0080 | — |
| `sigma_n_effective` | 1 | -0.0051 | truthfulqa_mc2 |
