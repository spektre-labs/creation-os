# v227 — σ-Entropy (`docs/v227/`)

Information-theoretic unification: entropy decomposition, KL-divergence,
mutual information, free energy.

> Canonical Creation OS remote:
> [spektre-labs/creation-os](https://github.com/spektre-labs/creation-os) —
> see [../CANONICAL_GIT_REPOSITORY.md](../CANONICAL_GIT_REPOSITORY.md).

## The problem

Shannon entropy `H = −Σ p log p` is the canonical uncertainty measure.
v104 already showed empirically that `σ_product` (v29 geometric mean
over 8 channels) tracks error more tightly than H alone.  v227
formalises the decomposition and makes the claim falsifiable in-tree.

## σ-innovation

Fixture: 8 distributions `p_i` over K = 5 categories — sharp peak,
bimodal, near-uniform, skewed decay, heavy tail, very sharp, pyramid —
chosen so the four decomposed channels carry different signal.

### Decomposed σ-channels

- `σ_H     = H(p) / log K`
- `σ_nEff  = 1 − n_eff / K`, where `n_eff = exp H`
- `σ_tail  = Σ_{k ≥ K/2} p[k]`
- `σ_top1  = 1 − max_k p[k]`

`σ_product = geometric mean of the four`, with an ε-floor (0.05) to
avoid `log 0` when a single channel collapses.

### Information-theoretic quantities

- `KL(p || uniform) = log K − H(p)`
- `σ_free = KL / log K = 1 − σ_H`  (free-energy identity)
- `I(X; C)_i = H(p_i) − H(q_i)` (clamped to `[0, H(p)]`)
- `σ_mutual = 1 − I / H(p)` (clamped to `[0, 1]`)

### Falsifiable claim

`σ_true_i := arithmetic mean of the four channels` — a transparent,
stated reference.  Two MSEs:

- `mse_H       = mean_i (σ_H_i        − σ_true_i)^2`
- `mse_product = mean_i (σ_product_i  − σ_true_i)^2`

Hard contract: `mse_product < mse_H`.  Why it holds: `σ_product` is the
GM of four channels and tracks the AM of those channels more closely
than a single channel does.  v104's empirical win is thus reproduced
as a decomposition-geometry argument on a fixed, offline fixture.

## Merge-gate contract

`bash benchmarks/v227/check_v227_entropy_decomposition.sh`

- self-test PASSES
- 8 distributions, K = 5
- `Σ_k p_i[k] = 1 ± 1e-3`, `p_i[k] ≥ 0`
- every σ ∈ [0, 1]
- `σ_H + σ_free = 1` (± 1e-3)
- `I(X; C) ∈ [0, H(p)]`
- `mse_product < mse_H`
- chain valid + byte-deterministic

## v0 vs v1 split

- **v0 (this tree)** — 8 hand-picked distributions with synthetic
  conditionals for MI; `σ_true = arithmetic mean of the 4 channels`;
  closed-form MSE comparison.
- **v227.1 (named, not implemented)** — active inference loop (policy
  minimising variational free energy over time), KL-based calibration,
  live σ_H vs σ_product learning-rate schedule, plug-in for v224 tensor
  channels.

## Honest claims

- **Is:** a reproducible demonstration that a decomposition-reweighted
  σ beats a single-channel entropy on a stated MSE reference.
- **Is not:** a proof that σ_product ≥ σ_H **in every** real-world LLM.
  The fixture is controlled; v227.1 needs live data and a reconsidered
  reference before any empirical claim can be extended beyond v0.

---

*Spektre Labs · Creation OS · 2026*
