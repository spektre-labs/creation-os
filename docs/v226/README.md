# v226 — σ-Attention (`docs/v226/`)

σ-interpretation of transformer attention: `Q = observer`, `K = observed`,
`V = meaning`, `softmax(Q·K^T / √d) = σ-distribution over keys`.

> Canonical Creation OS remote:
> [spektre-labs/creation-os](https://github.com/spektre-labs/creation-os) —
> see [../CANONICAL_GIT_REPOSITORY.md](../CANONICAL_GIT_REPOSITORY.md).

## The problem

The Creation OS paper reads attention as a σ-mechanism: the softmax is
exactly the observer-observed identity (`1 = 1`) normalised into a
distribution.  v226 operationalises this: each attention head gets a
per-token σ derived from the normalised Shannon entropy of its softmax
row, and each head gets a single σ_head by averaging over tokens.

## σ-innovation

For every head `h`, token `t`, key `k`:

- `qk[t][k]   = −|k − ideal_k(t, h)|`     (distance-to-preferred-key score)
- `softmax_t  = softmax(qk[t] / T_h)`     (per-head temperature)
- `σ_head_token = H(softmax_t) / log L`   (normalised entropy, L=key_len)
- `σ_head       = mean_t σ_head_token`

Heads are classified into three actions for downstream v177 / v180:

- `σ_head > τ_noisy  = 0.70` → **prune** (looks everywhere, not useful)
- `σ_head < τ_useful = 0.40` → **boost** (knows exactly where to look)
- otherwise                 → **keep**

v0 is read-only: no weights are modified — only classified.

### Deterministic fixture

- 8 heads × 6 tokens × key-length 6
- 2 'factual' heads (T = 0.20, 0.25) → σ_head ≪ τ_useful
- 2 'diffuse' heads (T = 2.20, 2.50) → σ_head ≫ τ_noisy
- 4 'mixed' heads in between
- preferred key per head: `(t + h) mod L`

## Merge-gate contract

`bash benchmarks/v226/check_v226_attention_per_head_sigma.sh`

- self-test PASSES
- 8 heads, 6 tokens, key-length 6
- softmax rows sum to 1 ± 1e-4 per (head, token)
- `σ_head_token`, `σ_head` ∈ [0, 1]
- `n_valuable ≥ 1` and `n_flagged ≥ 1` (heads really differ)
- `σ_head_max − σ_head_min ≥ 0.30` (not all alike)
- `n_boost + n_keep + n_prune == n_heads`
- chain valid + byte-deterministic

## v0 vs v1 split

- **v0 (this tree)** — synthetic `Q · K^T` from distance-to-preferred-key,
  per-head fixed temperature, classification only (no surgery).
- **v226.1 (named, not implemented)** — live transformer attention
  export, v180 attention-surgery wiring, `/attention` web dashboard with
  per-head real-time σ, causal per-head σ ablation, multi-layer tree
  aggregation.

## Honest claims

- **Is:** a reproducible per-head σ extracted from softmax entropy, with
  a deterministic classification into prune / boost / keep.
- **Is not:** a transformer training tool.  v0 never modifies any
  weight; v226.1 is where surgery lives and will need its own guards.

---

*Spektre Labs · Creation OS · 2026*
