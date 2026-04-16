# σ-Guided Test-Time Compute (v41 lab)

This note frames **Creation OS v41** as a **portable C lab**: σ decomposition (epistemic / aleatoric) drives *when* to inject continuation (“WAIT”), *when* to stop early, *how many* best-of-N samples to draw, and how aggressively to expand a toy reasoning tree.

## Evidence class (read before any headline)

- **Shipped in-tree today:** deterministic `make check-v41` policy tests + scaffolding (`src/v41/*`). Class: **lab demo (C)**.
- **Not shipped as a claim:** “2B BitNet beats 70B on GSM8K/AIME” without an archived harness + model bundle. That sentence is **P-tier** until `benchmarks/v41/` contains reproducible JSON and `docs/REPRO_BUNDLE_TEMPLATE.md` metadata.

## Relation to external “test-time compute” narratives

Papers and products discussing **budget forcing**, **self-correction**, and **test-time scaling** are cited only as **motivation**. This repository does **not** reproduce s1 / o1 / R1 numbers unless an evaluator is explicitly wired (see `benchmarks/v41/test_time_scaling.sh`).

## Footgun (integrators)

`v41_budget_next_action` treats **“model wants to stop but is still uncertain”** as `sigma_epistemic > sigma_continue_threshold` (inject WAIT). If your logits always yield very large epistemic values under the Dirichlet map, you must set `sigma_continue_threshold` **above that ceiling** (or fix logits scaling) or you will inject WAIT until `max_think_tokens`.

## Module map

| File | Role |
|------|------|
| `src/v41/budget_forcing.c` | σ_epistemic-aware WAIT injection + optional host `forward_logits` loop |
| `src/v41/self_verify.c` | Compare σ totals before/after a revision string (bookkeeping) |
| `src/v41/best_of_n.c` | σ_epistemic → adaptive N (0 = abstain on extreme uncertainty) |
| `src/v41/reasoning_tree.c` | Small branching tree; branch factor from decomposed σ on scratch logits |

## Commands

```bash
make check-v41
make bench-v41-scaling   # stub; exits 0 until RUN_V41_SCALING_HARNESS=1 + evaluator exists
```

## Working paper title (only when a harness exists)

**“σ-Guided Test-Time Compute: Adaptive Reasoning Depth via Epistemic Uncertainty Decomposition”**

Until then: treat v41 as **engineering interface + falsifiable policy**, not a leaderboard row.
