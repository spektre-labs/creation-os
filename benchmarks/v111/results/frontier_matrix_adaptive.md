# v111.2 adaptive / composite σ matrix

> **POST-HOC EXPLORATION — NOT PRE-REGISTERED.**  The signals
> in this matrix (`sigma_composite_max`, `sigma_composite_mean`,
> `sigma_task_adaptive`) were designed _after_ the v111
> pre-registered matrix was analysed.  They may NOT be
> substituted into the pre-registered claim.  Report alongside,
> never in place of, `frontier_matrix.md`.

- 3 post-hoc signals × 4 task families → Bonferroni N = 12
- α (post-hoc family-wise) = 0.05 / 12 = 0.00417
- n_boot = 2000

### arc_challenge — n=1172 · acc=0.4676 · entropy_AURCC=0.4672

| signal | AURCC ↓ | ΔAURCC vs entropy | CI95 | p | cov@acc≥0.90 | cov@acc≥0.95 | Bonferroni (post-hoc) |
|---|---:|---:|---|---:|---:|---:|:---:|
| `sigma_composite_max` | 0.4572 | -0.0101 | [-0.0319,+0.0109] | 0.3510 | 0.001 | 0.001 |  |
| `sigma_composite_mean` | 0.4562 | -0.0109 | [-0.0263,+0.0038] | 0.1570 | 0.002 | 0.002 |  |
| `sigma_task_adaptive` | 0.4585 | -0.0087 | [-0.0143,-0.0028] | 0.0040 | 0.003 | 0.003 | **yes** |


### arc_easy — n=2376 · acc=0.7551 · entropy_AURCC=0.1710

| signal | AURCC ↓ | ΔAURCC vs entropy | CI95 | p | cov@acc≥0.90 | cov@acc≥0.95 | Bonferroni (post-hoc) |
|---|---:|---:|---|---:|---:|---:|:---:|
| `sigma_composite_max` | 0.1691 | -0.0018 | [-0.0124,+0.0091] | 0.7550 | 0.096 | 0.028 |  |
| `sigma_composite_mean` | 0.1661 | -0.0048 | [-0.0119,+0.0023] | 0.1940 | 0.143 | 0.037 |  |
| `sigma_task_adaptive` | 0.1710 | +0.0000 | [+0.0000,+0.0000] | 2.0000 | 0.187 | 0.007 |  |


### truthfulqa_mc2 — n=817 · acc=0.4638 · entropy_AURCC=0.5528

| signal | AURCC ↓ | ΔAURCC vs entropy | CI95 | p | cov@acc≥0.90 | cov@acc≥0.95 | Bonferroni (post-hoc) |
|---|---:|---:|---|---:|---:|---:|:---:|
| `sigma_composite_max` | 0.5080 | -0.0447 | [-0.0708,-0.0189] | 0.0005 | 0.001 | 0.001 | **yes** |
| `sigma_composite_mean` | 0.5151 | -0.0376 | [-0.0573,-0.0183] | 0.0005 | 0.001 | 0.001 | **yes** |
| `sigma_task_adaptive` | 0.5080 | -0.0447 | [-0.0708,-0.0189] | 0.0005 | 0.001 | 0.001 | **yes** |


### hellaswag — n=746 · acc=0.4853 · entropy_AURCC=0.3796

| signal | AURCC ↓ | ΔAURCC vs entropy | CI95 | p | cov@acc≥0.90 | cov@acc≥0.95 | Bonferroni (post-hoc) |
|---|---:|---:|---|---:|---:|---:|:---:|
| `sigma_composite_max` | 0.3983 | +0.0187 | [-0.0009,+0.0375] | 0.0570 | 0.013 | 0.009 |  |
| `sigma_composite_mean` | 0.3921 | +0.0126 | [-0.0038,+0.0286] | 0.1350 | 0.017 | 0.011 |  |
| `sigma_task_adaptive` | 0.3796 | +0.0000 | [+0.0000,+0.0000] | 2.0000 | 0.034 | 0.028 |  |


### Cross-task post-hoc summary

| signal | Bonferroni (post-hoc) wins | mean ΔAURCC | tasks beaten |
|---|---:|---:|---|
| `sigma_composite_max` | 1 | -0.0095 | truthfulqa_mc2 |
| `sigma_composite_mean` | 1 | -0.0102 | truthfulqa_mc2 |
| `sigma_task_adaptive` | 2 | -0.0133 | arc_challenge, truthfulqa_mc2 |
