/* SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only */
/*
 * v157 σ-attention — sub-quadratic attention gated by per-token σ (Q16).
 *
 * Low σ ⇒ full pairwise attention over the sequence for that query row.
 * High σ ⇒ restricted attention to a sliding window (approximation / prune).
 *
 * Does **not** include or modify ``python/cos/sigma_gate.h``; numerics are local Q16.
 */
#ifndef CREATION_OS_INFERENCE_SIGMA_ATTENTION_H
#define CREATION_OS_INFERENCE_SIGMA_ATTENTION_H

#include <stdint.h>

#define COS_INFER_ATTN_Q16      ((int32_t)65536)
#define COS_INFER_ATTN_DIM_MAX  512

#ifdef __cplusplus
extern "C" {
#endif

typedef struct cos_infer_sigma_attention {
    int32_t  seq_len;
    int32_t  head_dim;
    int32_t  n_heads;
    int32_t *token_sigmas;
    int32_t  sigma_threshold_q16;
    uint32_t tokens_full;
    uint32_t tokens_pruned;
} cos_infer_sigma_attention_t;

void cos_infer_sigma_attention_init(cos_infer_sigma_attention_t *sa, int32_t seq_len,
                                    int32_t head_dim, int32_t n_heads, int32_t *token_sigmas,
                                    int32_t sigma_threshold_q16);

void cos_infer_sigma_attention_reset_stats(cos_infer_sigma_attention_t *sa);

/**
 * σ-sparse attention: writes ``output[seq_len * head_dim]``.
 * ``window_size`` half-width (inclusive neighbors); effective span roughly ``2*W+1`` keys per pruned row.
 */
void cos_infer_sigma_sparse_attention(cos_infer_sigma_attention_t *sa, const int32_t *Q,
                                      const int32_t *K, const int32_t *V, int32_t *output,
                                      int32_t window_size);

/**
 * KV cache row = ``2 * head_dim`` int32s (K slice then V slice). Removes highest-σ row by swapping
 * with the last live row. Updates ``sigmas`` in lockstep. Returns new logical length.
 */
int32_t cos_infer_sigma_kv_prune(cos_infer_sigma_attention_t *sa, int32_t *kv_cache,
                                 int32_t *sigmas, int32_t cache_len, int32_t head_dim,
                                 int32_t max_cache);

/**
 * Rough op count vs dense ``N*N*D`` multiply-adds (lab accounting only).
 */
void cos_infer_sigma_attention_ops_estimate(const cos_infer_sigma_attention_t *sa,
                                            int32_t seq_len, int32_t window_size, int32_t head_dim,
                                            int64_t *out_sparse_ops, int64_t *out_dense_ops);

#ifdef __cplusplus
}
#endif

#endif /* CREATION_OS_INFERENCE_SIGMA_ATTENTION_H */
