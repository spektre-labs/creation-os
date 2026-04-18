# v140 — σ-Causal

> v139 answers *what happens next*. v140 answers *what would happen IF*.

## Problem

A world model that only predicts the actual next state can't answer
counterfactual questions ("if I changed this component, what
follows?"). Nor can it *explain* a decision — "why did the model
abstain?". Both needs are central to safety, debuggability, and
interactive UX.

## σ-innovation

Two primitives composed on top of v139's linear `A`:

1. **Counterfactual propagation** (do-calculus, linear edition).
   Given `s_t`, a dim index `k`, and a value `v`:
   * `s_do = do_k(s_t, v)` (copy of `s_t` with component `k` overwritten).
   * `s_nat[t+1] = A · s_t`, `s_do[t+1] = A · s_do`.
   * `σ_causal = ‖s_do[t+1] − s_nat[t+1]‖ / ‖s_nat[t+1]‖` — the
     magnitude of the interventional effect.
2. **σ-channel ablation attribution.** The 8 σ-channels aggregate
   into a single `σ_agg = weighted_geomean(σ, w)`. For each channel
   `i`, replacing `σ[i]` with the neutral value 1.0 gives an ablated
   `σ_agg_i`; `Δ_i = σ_agg − σ_agg_i` is the channel's contribution
   to the final decision. The kernel returns the **top-3** channels
   by `|Δ|`, with percent-of-total attribution.

## Contract

| Surface                          | Input                                    | Output                                           |
| -------------------------------- | ---------------------------------------- | ------------------------------------------------ |
| `cos_v140_counterfactual`        | v139 model + state + dim + value         | Δ-norm + natural-norm + σ_causal                  |
| `cos_v140_weighted_geomean`      | weights + σ-values                        | scalar σ_agg ∈ (0, 1]                             |
| `cos_v140_attribute`             | weights + σ-values + threshold τ          | baseline σ_agg + verdict + top-3 channel Δ + %    |
| `cos_v140_attribution_to_json`   | attribution struct                        | canonical JSON                                   |

## Merge-gate measurements

| Metric                                  | v140.0         | Spec                    | Status |
| --------------------------------------- | -------------- | ----------------------- | ------ |
| Counterfactual Δ scales with perturbation | 2× → ≥ 1.5× Δ | monotone, ≥ 1.5× for 2× | ✅     |
| Top-3 attribution returned              | 3 entries      | K=3                     | ✅     |
| Near-neutral channel excluded from top-3 | yes           | |Δ| metric correct      | ✅     |
| JSON has `top` array + `verdict`         | yes           | shape                    | ✅     |

## Attribution semantic (read this)

v140.0's attribution is **|log-contribution|** of a channel to the
geometric-mean aggregator. In this metric, channels with σ *farthest
from the neutral point 1.0* dominate — regardless of verdict
direction. A channel with σ ≈ 1.0 (no signal) has |Δ| ≈ 0.

This means "the abstain verdict was 62% driven by n_effective" is
an *informal* paraphrase of "n_effective has the largest |log σ|
and hence moved σ_agg most". It is not a sign-aware pro-abstain
decomposition.

Sign-aware decomposition (splitting "pro-emit" vs "pro-abstain"
contributions around each channel's neutral crossing) is **v140.1**.

## v140.0 / v140.1 split

**v140.0 (shipped)**

* Counterfactual propagation on v139's linear `A`.
* σ-channel ablation with |Δ|-ranked top-3.
* Canonical JSON output.

**v140.1 (follow-up)**

* Sign-aware "pro-emit vs pro-abstain" attribution.
* `/v1/explain` HTTP endpoint on v106.
* Clickable σ-bar in v108 Web UI — each bar segment drills into the
  causal explanation.
* Structural intervention on the σ-aggregator itself (not just dim
  overwrite): replace the aggregator kind and measure the decision
  flip rate.

## Honest claims

* v140.0 is **linear-model causality** — it inherits v139's
  assumptions. Real-world non-linearity is a v140.1/v139.1 concern.
* The top-3 ranking is correct for a geomean aggregator. Other
  aggregators (additive, min/max) need a different Δ metric that is
  *not* provided by v140.0.
* σ_causal is a **magnitude**, not a sign. A larger σ_causal means
  "the intervention matters"; whether it matters pro-emit or
  pro-abstain requires the v140.1 decomposition.
