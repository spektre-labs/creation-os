/* SPDX-License-Identifier: AGPL-3.0-or-later
 * v34: extended σ channels (lab; see docs/SIGMA_VS_FIELD.md for literature mapping).
 */
#ifndef CREATION_OS_SIGMA_CHANNELS_V34_H
#define CREATION_OS_SIGMA_CHANNELS_V34_H

#include "decompose.h"

typedef struct {
    float predictive_entropy_seq;
    float semantic_entropy_cluster;
    float logtoku_aleatoric;
    float logtoku_epistemic;
    float logtoku_total;
    float critical_token_entropy;
    float top_margin;
    float eigenscore_hidden_proxy;
} sigma_channels_v34_t;

/** Mean −log p(y_t | x, y_<t) over supplied prefix (Kadavath-style sequence score). */
float sigma_predictive_entropy_mean(const float *neg_log_probs_per_token, int seq_len);

/**
 * Semantic entropy proxy: cluster N discrete top-1 token sequences by exact equality,
 * then normalized entropy of the empirical cluster distribution (embedding clustering is I-tier).
 */
float sigma_semantic_entropy_cluster_equal(const int *const *top1_token_ids, int n_samples, int seq_len);

/**
 * Weighted mean of per-token entropies on positions where critical_mask[i] != 0.
 * Mask is heuristic (digits, uppercase starts, non-space punctuation clusters).
 */
float sigma_critical_token_entropy_weighted(
    const float *per_token_entropy, const unsigned char *critical_mask, int n_tokens);

/** EigenScore proxy: hidden-state energy spread; returns 0 if hidden is NULL (no --expose-hidden). */
float sigma_eigenscore_hidden_proxy(const float *hidden, int dim);

void sigma_channels_v34_fill_last_step(const float *logits,
    int n_vocab,
    sigma_channels_v34_t *out);

#endif /* CREATION_OS_SIGMA_CHANNELS_V34_H */
