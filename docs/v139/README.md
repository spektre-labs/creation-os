# v139 — σ-WorldModel

> LLMs predict the next *token*. A world model predicts the next *state*.

## Problem

A chat model that only predicts tokens has no internal theory of the
world. It cannot answer "is this situation familiar?" or "will this
plan remain stable over the next N steps?" — both of which are
prerequisites for robust planning, exploration, and σ-gated
abstention on novel inputs.

## σ-innovation

v139 fits a **linear transition** `A ∈ ℝ^{D×D}` from a sequence of
latent states `s_0, s_1, …, s_T`, then uses `A` to:

1. **Predict** the next state: `s_{t+1}^pred = A · s_t`.
2. **Measure surprise** as
   `σ_world = ‖s_{t+1}^actual − s_{t+1}^pred‖ / ‖s_{t+1}^actual‖`.
3. **Roll out** N steps forward in latent space and return the
   per-step `σ_world` trajectory plus a *monotonically-rising* flag
   — the "this plan is breaking down" signal for v121 HTN planning.

## Contract

| Surface                    | Input                       | Output                                    |
| -------------------------- | --------------------------- | ----------------------------------------- |
| `cos_v139_fit`             | D, n pairs (n > D)          | fitted model + training residual           |
| `cos_v139_predict`         | current state               | predicted next state                       |
| `cos_v139_sigma_world`     | actual + predicted          | relative residual ∈ [0, ∞)                 |
| `cos_v139_rollout`         | seed state + horizon ≤ 32   | N predicted states + σ_world + monotone flag |

## Merge-gate measurements

Tier-0 synthetic: D=16, T=400, ρ=1.0 (orthogonal rotation).

| Metric                  | v139.0  | Spec          | Status |
| ----------------------- | ------- | ------------- | ------ |
| Training residual       | ~1e-7   | < 10%         | ✅     |
| Held-out σ_world (mean) | ~1e-7   | < 10%         | ✅     |
| 8-step rollout max σ    | 0.0     | < 30%         | ✅     |
| Surprise detection      | ≥ 3×    | > 3× familiar | ✅     |
| JSON shape              | present | required      | ✅     |

## v139.0 / v139.1 split

**v139.0 (shipped)**

* Pure C, float + double, Gauss-Jordan with Tikhonov regularisation.
* D up to 64.
* Orthogonal-basis synthetic trajectories for the self-test.
* Multi-step rollout with σ_world trajectory.

**v139.1 (follow-up)**

* D = 2560 backed by **real BitNet layer-15 hidden states** via
  v101/v126.
* Rank-r truncated SVD compression (`models/v139/world_model_lr64.bin`,
  ~640 KB).
* Streaming fit (incremental normal-equations update) so the model
  tracks distribution drift.
* `/v1/plan` integration: each HTN step receives its σ_world and the
  HTN branch pruning uses a monotone-rising cutoff.

## Honest claims

* v139.0 is a **linear** world model. Anything nonlinear — recursion,
  gating, attention over state history — is out of scope until v139.1.
* Synthetic-trajectory merge-gate numbers are near-machine-zero
  because orthogonal rotation is *exactly* recoverable by LS. This
  is a correctness proof for the fit + predict pipeline, not a claim
  about BitNet hidden-state prediction quality.
* `σ_world` is a **residual**, not a probability. It's comparable
  *within a deployment* (trend over time), not across deployments
  (D or A-norm dependent).
