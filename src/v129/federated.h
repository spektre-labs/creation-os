/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (c) 2026 Spektre Labs / Lauri Rainio
 *
 * v129 σ-Federated — privacy-preserving collaborative learning.
 *
 * Composes with:
 *   - v125 σ-DPO   — each node trains locally, σ-DPO emits LoRA Δ
 *   - v124 σ-Cont. — the per-node epoch that produced the delta
 *   - v128 mesh    — the transport layer (v128 is the follow-up;
 *                    v129 operates on pre-collected (delta, σ) tuples)
 *
 * Four σ-innovations vs plain FedAvg (McMahan et al. 2017):
 *
 *   1. σ-weighted FedAvg.  Standard FedAvg averages node deltas by
 *      dataset-size weights.  v129 weights by 1/σ_node so confident
 *      nodes dominate the aggregate and uncertain nodes cannot
 *      corrupt the global model.  Concretely, weight_i ∝ 1/(ε + σ_i)
 *      normalized to sum to one.
 *
 *   2. σ-scaled differential privacy.  Gaussian noise with σ_DP
 *      = (base_noise · (1 + α · σ_node)) is added per coordinate
 *      before sharing: high-σ nodes get *more* noise (uncertain
 *      update → more protection from inference attacks).
 *
 *   3. σ-adaptive top-K sparsification.  K = clamp(K_min, K_max,
 *      round(K_base · (1 − σ_node))).  Confident nodes share more
 *      coordinates; uncertain nodes share fewer — less signal, less
 *      exposure, less bandwidth.
 *
 *   4. σ-tape unlearning.  Given a previously-aggregated global
 *      delta and the specific contribution to invert, the node
 *      subtracts `weight_i · contribution_delta` from the global
 *      state and re-emits a diff for the other nodes to re-apply.
 *      Composes with v6 σ-tape (Creation OS's recovery checkpoints)
 *      for GDPR-compliant erasure-of-influence.
 *
 * v129.0 (this kernel) is the pure-C aggregator + DP-noise kernel
 * + sparsifier + unlearn-diff.  It deliberately does not open a
 * socket — the transport is v128 mesh (follow-up), and keeping
 * v129.0 transport-free lets the merge-gate exercise the
 * aggregation *math* deterministically on any runner.
 */
#ifndef COS_V129_FEDERATED_H
#define COS_V129_FEDERATED_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V129_EPS_SIGMA            1e-4f
#define COS_V129_ALPHA_DP_DEFAULT     1.00f    /* noise grows with σ */
#define COS_V129_BASE_NOISE_DEFAULT   0.01f    /* per-coord stddev */
#define COS_V129_K_BASE_DEFAULT       64
#define COS_V129_K_MIN_DEFAULT         4
#define COS_V129_K_MAX_DEFAULT       256

typedef struct cos_v129_node_delta {
    int      node_id;
    float    sigma_node;     /* node's avg σ_product over its epoch */
    float   *delta;          /* flattened LoRA delta, size dim */
    int      dim;
    float    epsilon_spent;  /* DP budget already consumed */
} cos_v129_node_delta_t;

typedef struct cos_v129_config {
    float alpha_dp;          /* 1.00 */
    float base_noise;        /* 0.01 */
    int   k_base;            /* 64 */
    int   k_min;             /* 4 */
    int   k_max;             /* 256 */
} cos_v129_config_t;

typedef struct cos_v129_aggregate_stats {
    int   n_nodes;
    int   dim;
    float weight_min;
    float weight_max;
    float weight_sum;        /* should normalize to 1.0 */
    int   winner_node_id;    /* node with largest weight */
    float mean_abs_delta;    /* mean |out_delta[i]| after aggregation */
} cos_v129_aggregate_stats_t;

void cos_v129_config_defaults(cos_v129_config_t *cfg);

/* Compute σ-weighted FedAvg weights (normalized, sum to 1).
 * weights[i] ∝ 1 / (EPS_SIGMA + σ_i). */
int  cos_v129_compute_weights(const cos_v129_node_delta_t *nodes,
                              int n, float *out_weights);

/* Aggregate: out[k] = Σ_i w_i · nodes[i].delta[k].  Returns 0 on ok,
 * -1 on bad dims. */
int  cos_v129_aggregate(const cos_v129_node_delta_t *nodes, int n,
                        float *out_delta, int dim,
                        cos_v129_aggregate_stats_t *stats);

/* Deterministic PRNG for reproducible DP: SplitMix64 wrapper. */
uint64_t cos_v129_rng_splitmix64(uint64_t *state);

/* Box-Muller Gaussian N(0,1).  Uses *state as SplitMix64 seed. */
float    cos_v129_rng_gauss(uint64_t *state);

/* Add per-coordinate Gaussian noise whose stddev is
 *   σ_DP = base_noise · (1 + α_dp · σ_node) · (1/ε_budget)
 * (ε_budget clamped to [0.01, ∞)).  Returns the effective σ_DP. */
float    cos_v129_add_dp_noise(float *delta, int dim,
                               float sigma_node, float epsilon_budget,
                               const cos_v129_config_t *cfg,
                               uint64_t *rng_state);

/* σ-adaptive K from σ_node.  K = clamp(k_min, k_max,
 * round(k_base · (1 − σ_node))). */
int  cos_v129_adaptive_k(const cos_v129_config_t *cfg, float sigma_node);

/* Top-K sparsify by |value|.  Writes into out_idx / out_val in
 * descending |delta[idx]| order.  Returns number written (≤ K). */
int  cos_v129_sparsify_topk(const float *delta, int dim, int k,
                            int *out_idx, float *out_val);

/* Unlearning diff: compute the Δ you must *subtract* from the
 * current global state to erase `contribution` that was merged at
 * weight `w`.  Returns 0 on ok, -1 on bad dims. */
int  cos_v129_unlearn_diff(const float *contribution, int dim,
                           float weight, float *out_diff);

int  cos_v129_stats_to_json(const cos_v129_aggregate_stats_t *s,
                            char *out, size_t cap);

int  cos_v129_self_test(void);

#ifdef __cplusplus
}
#endif
#endif
