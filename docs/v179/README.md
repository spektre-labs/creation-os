# v179 σ-Interpret — mechanistic interpretability for σ

## Problem

σ tells us **how uncertain** a model is from the outside. The
interpretability literature tells us **what the model is
computing** on the inside. Nothing so far joins the two: when
σ rises, we cannot say *why* it rose — which neuron, which
head, which token.

## σ-innovation

v179 decomposes the σ-signal mechanistically:

1. **SAE decomposition.** A Sparse Autoencoder sits on layer 15
   (the same layer v126 embeds) and reconstructs the activation
   as a sum of monosemantic features. v179.0 ships a
   deterministic 24-feature fixture with eight seeded
   uncertainty modes — `uncertainty-about-dates`,
   `numerical-ambiguity`, `low-training-coverage`,
   `rare-token-fallback`, etc. — and 16 noise distractors.
2. **σ-correlation filter.** Only features with `|Pearson r|
   ≥ τ_correlated = 0.60` against `σ_product` are kept. The
   fixture yields ≥ 5 correlated features; the top-r feature
   is `uncertainty-about-dates` with `r ≈ 0.82`.
3. **σ-circuit tracing.** A σ-rise at a token is traced back
   to exactly one attention head + one MLP neuron + one input
   token position. v179.0 records the chain deterministically;
   v179.1 computes it on real activations.
4. **v140 bridge.** The channels that collapsed (`n_eff`
   before → after) are the same v140 channels responsible for
   the uncertainty — v179 explains *why*, v140 explains
   *which*.
5. **Human-readable explain.** `./creation_os_v179_interpret
   --explain N` returns a single string mentioning σ, the SAE
   feature id and label, the driving head/MLP/token, and the
   n_effective collapse.

## Merge-gate

`make check-v179` runs
`benchmarks/v179/check_v179_sae_feature_correlation.sh` and
verifies:

- self-test
- `n_features_correlated ≥ 5`
- top feature `|r| ≥ 0.60` and carries a non-empty,
  non-`unlabeled-feature` label
- `--explain N` for a high-σ sample yields a non-empty
  explanation mentioning the feature id (`#<fid>`) and
  `sigma=`, and an `n_eff_after < n_eff_before` collapse
- output byte-deterministic

## v179.0 (shipped) vs v179.1 (named)

| | v179.0 | v179.1 |
|-|-|-|
| SAE weights | deterministic fixture | 2560 → 8192 SAE over BitNet-2B layer 15 |
| Circuit trace | baked coordinates | live attention + MLP back-trace |
| v140 wiring | shared n_eff shape | causal channel id fan-out |
| Endpoint | CLI JSON | `GET /v1/explain/{response_id}` + web UI σ-timeline |
| EU AI Act | contract shape | submitted auditable explanations |

## Honest claims

- **Is:** a deterministic, offline SAE-style decomposition that
  produces named uncertainty features and mechanistic
  explanations with σ, token, head, MLP, and n_effective
  collapse.
- **Is not:** a real SAE trained on live weights. No activation
  hooks run; no real correlations are learned. Real SAE
  training, real circuit tracing, and the web endpoint ship in
  v179.1.
