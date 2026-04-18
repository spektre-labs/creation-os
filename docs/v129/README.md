# v129 — σ-Federated

**Privacy-preserving collaborative learning with σ-weighted FedAvg.**

## Problem

Plain federated averaging (McMahan et al. 2017) weights each node by dataset size.  A noisy, uncertain node — one whose local σ tells us its gradients are low-quality — is allowed to corrupt the global model at the same weight as a confident one.

## σ-innovation

Four mechanisms, none of which exist in stock FedAvg:

1. **σ-weighted FedAvg.**  Weight_i ∝ 1 / (ε + σ_i), normalised to sum to one.  Confident nodes dominate; uncertain ones barely move the aggregate.  To my knowledge this specific weighting (aggregate-level σ instead of sample-level importance) is novel in the federated-learning literature.
2. **σ-scaled differential privacy.**  Gaussian noise of stddev `σ_DP = base · (1 + α · σ_node) / ε_budget` is added before sharing.  Uncertain updates get more noise (more protection; less signal, but less leakage).
3. **σ-adaptive top-K sparsification.**  `K = clamp(K_min, K_max, round(K_base · (1 − σ_node)))`.  Confident nodes share more coordinates; uncertain nodes share fewer (lower bandwidth + lower leakage).
4. **Unlearn-diff.**  Given the contribution a node made at weight `w`, subtract `w · contribution` from the global state to erase its influence.  Composes with v6 σ-tape for GDPR-compliant "right to be forgotten."

## Contract

```c
cos_v129_compute_weights(nodes, n, w)     // w_i ∝ 1 / (ε + σ_i), Σ w_i = 1
cos_v129_aggregate(nodes, n, out, dim, &stats)
cos_v129_add_dp_noise(delta, dim, σ_node, ε, cfg, &rng_state)
cos_v129_adaptive_k(cfg, σ_node)
cos_v129_sparsify_topk(delta, dim, k, idx_out, val_out)
cos_v129_unlearn_diff(contribution, dim, weight, out_diff)
```

All helpers are deterministic — RNG is SplitMix64 + Box-Muller.

## What's measured by `check-v129`

- Aggregation weights sum to 1.0 and the lowest-σ node becomes winner.
- σ_DP monotonically grows with σ_node at fixed ε.
- K_confident > K_uncertain (adaptive top-K).
- Unlearn round-trip: aggregate, apply unlearn-diff, recover the left-out node's contribution exactly.
- Gaussian RNG has mean ≈ 0 and variance ≈ 1 over 4096 samples.

## What ships now vs v129.1

| v129.0 (this commit) | v129.1 (follow-up) |
|---|---|
| Pure-C aggregator + DP noise + top-K + unlearn-diff | v128 mesh transport + handshake / authentication |
| σ-weighted FedAvg math | 2-node measured end-to-end round-trip + bench |
| Deterministic SplitMix64 / Box-Muller RNG | PyTorch / MLX LoRA-gradient extraction bridge |

v129.0 is transport-free by design so the merge-gate can exercise the aggregation math on any runner without a network stack.  v128 mesh plugs the socket layer in without touching v129.0's contract.

## Composition map

```
v124 σ-continual  ──┐                       ┌──► other v128 peers
                    ├─► v125 σ-DPO (local) ─┼─► v129 σ-federated aggregate
v115 σ-memory ──────┘  local gradient Δ     └──► GDPR unlearn-diff
                                              (subtract contribution)
```
