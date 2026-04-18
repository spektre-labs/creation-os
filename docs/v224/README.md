# v224 — σ-Tensor (`docs/v224/`)

Tensor-network representation of the 8 σ-channels, with contraction,
low-rank compression, and channel-correlation modelling.

> Canonical Creation OS remote:
> [spektre-labs/creation-os](https://github.com/spektre-labs/creation-os) —
> see [../CANONICAL_GIT_REPOSITORY.md](../CANONICAL_GIT_REPOSITORY.md).

## The problem

v29 treats the 8 σ-channels (entropy, `n_effective`, `tail_mass`,
`logit_std`, `top1_margin`, `logprob_gap`, `agreement`, `renorm_ratio`) as
independent and reduces them with a geometric mean.  They are not
independent — `entropy ↔ n_effective` and `tail_mass ↔ logit_std` are
well-known covariate pairs.  v224 models these correlations explicitly.

## σ-innovation

The σ-channels are lifted to a **rank-3 σ-tensor**
`T[t, c, k] ∈ R^{T × C × K}` with `T = 6` (tokens) × `C = 8` (channels) ×
`K = 3` (context buckets).  The fixture synthesises `T` from two latent
signals `a(t, k)` and `b(t, k)` through per-channel loadings, so
channels 0..3 co-vary through `a` and channels 4..7 co-vary through `b`
by construction — the 8×8 channel-correlation matrix is approximately
block-rank-4.

### Contraction σ-aggregation

Instead of the geometric mean, v224 reports a **tensor contraction**:

- `σ_agg[t]   = Σ_c w_c · mean_k T[t, c, k]`
- `σ_gmean[t] = ( Π_c mean_k T[t, c, k] )^{1/C}`

Both are reported so downstream optimisers (v136) can choose.  `σ_agg`
differs from `σ_gmean` on ≥ 1 token by construction because the
contraction picks up a channel correlation the geometric mean cannot see.

### Low-rank compression

The `C × C` channel-correlation matrix `R` is approximated by the top
`k = 4` eigen-components via symmetric power iteration (40 iters per
vector, deterministic init, rank-1 deflation).  The relative Frobenius
reconstruction error `rel_err = ‖R − R_k‖_F / ‖R‖_F` must be ≤ τ_rel
(0.15).  Storage:

- `params_full    = C · C             = 64`
- `params_lowrank = k · (C + 1)       = 36`

i.e. ≈ 44 % of full storage for ≤ 15 % reconstruction error.

## Merge-gate contract

`bash benchmarks/v224/check_v224_tensor_contraction.sh`

- self-test PASSES
- tensor shape `6 × 8 × 3`, entries ∈ [0, 1]
- weight vector `w` sums to 1, entries ∈ [0, 1]
- `σ_agg`, `σ_gmean` ∈ [0, 1] per token
- `|σ_agg − σ_gmean| ≤ δ_corr = 0.50` per token
- `rel_err ≤ τ_rel = 0.15`
- `params_lowrank < params_full`
- `n_divergent ≥ 1` (contraction actually diverges from GM on ≥ 1 token)
- chain valid + byte-deterministic

## v0 vs v1 split

- **v0 (this tree)** — synthetic rank-3 σ-tensor with engineered block
  structure, symmetric power iteration on the channel correlation, mean
  aggregation over context buckets, memset-zeroed hash records.
- **v224.1 (named, not implemented)** — true SVD on a live σ-history
  tensor, v136 CMA-ES over the channel weight vector, MPO / tree-tensor
  contraction for long-context σ, and v177 compress integration.

## Honest claims

- **Is:** a deterministic demonstration that a rank-3 σ-tensor with a
  rank-4 correlation approximation gives a strictly cheaper storage
  (36 vs 64 floats) at ≤ 15 % reconstruction error, and that tensor
  contraction is a different σ-aggregator from the geometric mean by
  construction.
- **Is not:** quantum hardware.  Tensor networks share formalism with
  quantum simulation; running on a classical CPU gives classical cost.

---

*Spektre Labs · Creation OS · 2026*
