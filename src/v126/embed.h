/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (c) 2026 Spektre Labs / Lauri Rainio
 *
 * v126 σ-Embed — σ-aware embedding for the v115 memory store.
 *
 * v115 memory ranks a query q against a stored memory m with:
 *
 *     score(q, m) = cosine(q, m) / (1 + λ · σ_product(m))
 *
 * using 384-d generic embeddings (MiniLM placeholder).  That works
 * as a rank on *content* but throws away the σ_profile that every
 * other kernel in Creation OS already carries.  v126 replaces it
 * with a hybrid vector:
 *
 *     v126_embed(x) = [  hidden_2560(x)  |  σ_weight · σ_8(x)  ]  ∈ ℝ^2568
 *
 * The 2560-d block is the hidden state at layer 15/30 of the BitNet
 * 2B model — i.e. no separate embedding network, the generator
 * *is* the retriever.  v126.0 (this kernel) ships a deterministic
 * hash-shingled 2560-d projector so the merge-gate can exercise the
 * contract without weights.  v126.1 swaps it for the actual BitNet
 * layer-15 extractor exposed by the v101 bridge.
 *
 * The 8-channel σ block is v101's 8-channel σ_profile scaled by
 * `sigma_weight` (default 0.50).  Because cosine similarity is
 * over the *whole* 2568-d vector, two memories with identical
 * content but divergent σ_profiles are no longer collapsed into the
 * same neighborhood — the retriever can see uncertainty.
 *
 * Ranking extends v115's weighting unchanged:
 *
 *     score(q, m) = cosine_2568(q, m) / (1 + λ · σ_product(m))
 *
 * so "uncertain memories are down-weighted, not silently mixed into
 * context" remains true — now with σ also contributing to the dot
 * product itself.
 *
 * This kernel is the embedding composer, the cosine + scoring
 * kernel, and the self-test that proves the advertised behaviors:
 * identical-content + identical-σ → similarity 1, identical-content
 * + divergent-σ → similarity < 1, different-content + same-σ →
 * similarity dominated by content, σ-weighted score down-ranks
 * high-σ memories.
 */
#ifndef COS_V126_EMBED_H
#define COS_V126_EMBED_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V126_HIDDEN_DIM     2560
#define COS_V126_SIGMA_DIM         8
#define COS_V126_EMBED_DIM      (COS_V126_HIDDEN_DIM + COS_V126_SIGMA_DIM)

#define COS_V126_SIGMA_WEIGHT_DEFAULT     0.50f
#define COS_V126_MEM_PENALTY_DEFAULT      1.00f   /* λ in v115 formula */

/* v101-compatible σ_profile.  Channels (canonical order):
 *   0: σ_coherence   — layer-to-layer cosine variance
 *   1: σ_probability — token-prob margin
 *   2: σ_length      — length/entropy deviation
 *   3: σ_repetition  — n-gram repeat score
 *   4: σ_entropy     — next-token entropy
 *   5: σ_domain      — domain mismatch estimator
 *   6: σ_retrieval   — RAG top-k score spread
 *   7: σ_tool        — tool-call schema conformance
 * σ_product = geometric mean of channels (cached). */
typedef struct cos_v126_sigma_profile {
    float channels[COS_V126_SIGMA_DIM];
    float sigma_product;
} cos_v126_sigma_profile_t;

typedef struct cos_v126_embedding {
    float v[COS_V126_EMBED_DIM];  /* 2568 floats */
} cos_v126_embedding_t;

typedef struct cos_v126_config {
    float sigma_weight;                /* 0.50 */
    float memory_uncertainty_penalty;  /* 1.00 (λ) */
} cos_v126_config_t;

void cos_v126_config_defaults(cos_v126_config_t *cfg);

/* Fill a sigma_profile from 8 raw channel readings; recomputes the
 * geometric-mean σ_product.  Channels must be in [0,1]. */
void cos_v126_sigma_fill(cos_v126_sigma_profile_t *p,
                         const float channels[COS_V126_SIGMA_DIM]);

/* Deterministic hash-shingled BitNet-layer-15 *stand-in*.
 * v126.0 implementation: lowercase + tokenize on whitespace + fold
 * each token's SplitMix64 hash into 4 pseudo-slots in the 2560-d
 * vector, then L2-normalize.  Pure C, no deps.  Replaced by the
 * real BitNet extractor in v126.1 behind the same API. */
void cos_v126_hidden_from_text(const char *text,
                               float *hidden_out  /* dim 2560 */);

/* Compose the full 2568-d embedding.  Does NOT re-normalize the
 * composite vector — the σ_weight knob controls the relative
 * contribution of the σ block to the subsequent cosine. */
void cos_v126_embed(const cos_v126_config_t *cfg,
                    const char *text,
                    const cos_v126_sigma_profile_t *sigma,
                    cos_v126_embedding_t *out);

/* Plain cosine over 2568 dims.  Returns 0 on zero vectors. */
float cos_v126_cosine(const cos_v126_embedding_t *a,
                      const cos_v126_embedding_t *b);

/* v115-style score: cosine / (1 + λ · σ_product(m)). */
float cos_v126_score(const cos_v126_config_t *cfg,
                     const cos_v126_embedding_t *q,
                     const cos_v126_embedding_t *m,
                     const cos_v126_sigma_profile_t *m_sigma);

/* Rank: given a query and `n` memories, fill `out_idx[0..k)` with
 * indices of the top-k by σ-weighted score.  Stable ordering on
 * ties (by index).  Returns number of entries written. */
int cos_v126_rank_topk(const cos_v126_config_t *cfg,
                       const cos_v126_embedding_t *q,
                       const cos_v126_embedding_t *memories,
                       const cos_v126_sigma_profile_t *m_sigmas,
                       int n,
                       int *out_idx,
                       float *out_scores,
                       int k);

int  cos_v126_self_test(void);

#ifdef __cplusplus
}
#endif
#endif  /* COS_V126_EMBED_H */
