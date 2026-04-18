# v136 σ-Evolve — Gradient-free architecture search for σ

Source: `src/v136/evolve.[ch]` · `src/v136/main.c`
Gate:   `benchmarks/v136/check_v136_evolve_cmaes_converge.sh`
Build:  `make creation_os_v136_evolve` · `make check-v136`

## Problem

v29 aggregates eight σ-channels into a single `σ_product`. v104
selected the geometric mean by **hand**. The question v136 asks:

> Given an archived sidecar of `(channels, should_abstain)` pairs,
> what per-channel weight vector and τ threshold give the best
> balanced accuracy on the σ-gate decision?

Neural fine-tuning would need gradients of a non-differentiable
decision rule. Grid-search would need tight discretisation. Both
are wrong tools.

## σ-innovation

v136 runs a **deterministic (μ/μ, λ) Evolution Strategy with
diagonal covariance adaptation** — a consciously simplified
member of the CMA-ES family — on the σ-aggregator genotype itself.
Gradient-free. No GPU. Fits in ~250 lines of C.

### Genotype (9 reals)

| | Range | Role |
|---|---|---|
| `w[0..7]` | `[0, 1]` | per-channel weight |
| `tau`     | `[0, 1]` | abstain threshold |

### Phenotype

```
sigma_aggr(x) = exp(Σ w_i · log(x_i) / Σ w_i)      (weighted geo mean)
verdict(x)    = sigma_aggr(x) > tau ? ABSTAIN : EMIT
```

### ES step

1. Evaluate λ offspring against the sidecar: fitness = balanced
   accuracy = `0.5 · TPR + 0.5 · TNR`.
2. Sort by fitness, keep top μ.
3. Fit `N(centroid, diag(var) · σ_mut²)` over the parents.
4. Resample λ offspring from that Gaussian, clip to `[0, 1]`.
5. Elitism: best-so-far is always a parent → trajectory is
   **monotone non-decreasing**.

### Synthetic sidecar (`cos_v136_synthetic`)

| Channels | Distribution |
|----------|--------------|
| 0..4 informative | `N(0.30, 0.10)` for emit, `N(0.55, 0.10)` for abstain |
| 5..7 noise        | `U(0, 1)` regardless of label |

Under this data a uniform-weights aggregator scores ≈ **0.74**;
the ES converges to ≈ **0.98** by learning `w[5..7] ≈ 0` and
placing `τ ≈ 0.42`.

## Output

`cos_v136_write_toml(path)` emits a TOML block v29 can consume
at runtime:

```toml
[sigma_aggregator]
kind    = "weighted_geometric"
tau     = 0.415
weights = [0.94, 0.81, 0.60, 0.52, 0.63, 0.33, 0.04, 0.35]
fitness = 0.985
```

## Merge-gate measurements

| Claim | Check |
|-------|-------|
| Evolved fitness − baseline fitness ≥ **0.02** | `check-v136` |
| Evolved fitness > **0.80** absolute | `check-v136` |
| Trajectory[i] ≥ Trajectory[i−1] (elitism invariant) | `check-v136` |
| TOML output contains `[sigma_aggregator]` + `kind = "weighted_geometric"` + `weights = […]` | `check-v136` |

## v136.0 vs v136.1

| | Shipped (v136.0) | Follow-up (v136.1) |
|---|---|---|
| (μ/μ, λ)-ES + diagonal covariance | ✅ | Full CMA-ES (rank-1 + rank-μ) |
| Synthetic sidecar | ✅ | Real archived σ-logs from v104 / v106 |
| TOML output for v29 pickup | ✅ | `make compile-sigma` composes with v137 |

## Honest claims

* The shipped optimiser is **CMA-ES-family**, not full CMA-ES. Rank-1
  and rank-μ covariance updates are v136.1; the diagonal version
  already hits 0.98 fitness on the in-tree synthetic.
* The "100 s total compute on M3" budget in the spec refers to a
  20×50 population × 0.1 s fitness evaluation; our in-tree run
  completes in **< 1 second** because the synthetic sidecar is
  tiny (N=200) and evaluation is a single weighted geo-mean per
  sample. Real σ-logs will push that up, but the budget stays.
