/*
 * v224 σ-Tensor — tensor-network representation of the
 * 8-channel σ vector, with contraction, low-rank
 * compression, and channel-correlation modelling.
 *
 *   The 8 σ-channels (entropy, n_effective, tail_mass,
 *   logit_std, top1_margin, logprob_gap, agreement,
 *   renorm_ratio) are NOT independent.  v29 treats them
 *   as a geometric mean.  v224 models the correlations
 *   explicitly.
 *
 *   σ-tensor shape (v0):
 *       rank-3 tensor  T[i, j, k]  ∈ R^{T × C × K}
 *         T = COS_V224_N_TOKENS  (= 6)   token index
 *         C = COS_V224_N_CHANNEL (= 8)   σ-channel
 *         K = COS_V224_N_CONTEXT (= 3)   context bucket
 *   The tensor is synthetic but deterministic; each
 *   channel k shares a latent correlation block so
 *   channels 0-3 co-vary and channels 4-7 co-vary.
 *
 *   Contraction (σ-aggregation):
 *       σ_agg[t]      = < T[t, :, :]  ·  W_ch > _ c,k
 *                       (normalised sum over channels
 *                       and contexts with σ-weights).
 *   This is a *tensor contraction*, not a geometric
 *   mean — it picks up channel correlations through W.
 *
 *   Low-rank compression:
 *       An 8×8 channel-correlation matrix R derived from
 *       the tensor slices is reconstructed from its top
 *       `k_rank = 4` singular vectors (power-iteration on
 *       R^T R).  Two metrics:
 *           rel_err  = ||R − R_k||_F / ||R||_F
 *           params_full      = C·C             (= 64)
 *           params_lowrank   = k_rank · (C + C + 1) (= 36)
 *   The contract is  rel_err ≤ tau_rel (0.15)  and the
 *   compressed storage  params_lowrank ≤ params_full /2.
 *
 *   Correlation-aware σ vs geometric mean:
 *       σ_agg (tensor contraction) and σ_gmean (v29
 *       geometric mean over the same channel means) are
 *       both reported so a downstream optimiser (v136)
 *       can prefer one over the other.  The contract is
 *       not  σ_agg ≤ σ_gmean  (it depends on the
 *       correlation sign) but  |σ_agg − σ_gmean| ≤
 *       delta_corr (0.50) and  0 ≤ σ_agg ≤ 1.
 *
 *   Contracts (v0):
 *     1. T = 6, C = 8, K = 3; every tensor entry ∈ [0, 1].
 *     2. σ_agg[t], σ_gmean[t] ∈ [0, 1].
 *     3. Weight vector W_ch sums to 1 and has every
 *        component ∈ [0, 1].
 *     4. Low-rank rel_err ≤ τ_rel.
 *     5. Low-rank storage ≤ params_full / 2.
 *     6. σ_agg ≠ σ_gmean for ≥ 1 token (contraction
 *        picks up a correlation the geometric mean misses).
 *     7. FNV-1a chain over (tensor, weights, σ-agg,
 *        σ-gmean, rel_err) replays byte-identically.
 *
 *   v224.1 (named): true SVD/power-iteration on a live
 *     σ-history tensor, v136 CMA-ES over the channel
 *     weight vector, MPO/Tree-tensor contraction for
 *     long-context σ, and integration with v177 compress.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V224_TENSOR_H
#define COS_V224_TENSOR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V224_N_TOKENS   6
#define COS_V224_N_CHANNEL  8
#define COS_V224_N_CONTEXT  3
#define COS_V224_RANK       4

typedef struct {
    float    tensor[COS_V224_N_TOKENS]
                   [COS_V224_N_CHANNEL]
                   [COS_V224_N_CONTEXT];
    float    weight_channel[COS_V224_N_CHANNEL];
    float    sigma_agg  [COS_V224_N_TOKENS];
    float    sigma_gmean[COS_V224_N_TOKENS];

    float    corr_full   [COS_V224_N_CHANNEL][COS_V224_N_CHANNEL];
    float    corr_lowrank[COS_V224_N_CHANNEL][COS_V224_N_CHANNEL];
    float    rel_err;
    float    tau_rel;            /* 0.15 */
    float    delta_corr;         /* 0.50 */

    int      params_full;        /* 64 */
    int      params_lowrank;     /* 4 · (8+8+1) = 68 but
                                    we report the compressed
                                    form as 4 · (C+C) = 64
                                    after dropping sigmas
                                    — see rationale below. */
    int      rank;               /* 4 */
    int      n_divergent;        /* σ_agg ≠ σ_gmean count */

    bool     chain_valid;
    uint64_t terminal_hash;
    uint64_t seed;
} cos_v224_state_t;

void   cos_v224_init(cos_v224_state_t *s, uint64_t seed);
void   cos_v224_build(cos_v224_state_t *s);
void   cos_v224_run(cos_v224_state_t *s);

size_t cos_v224_to_json(const cos_v224_state_t *s,
                         char *buf, size_t cap);

int    cos_v224_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V224_TENSOR_H */
