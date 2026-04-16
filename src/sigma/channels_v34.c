/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "channels_v34.h"

#include "channels.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

float sigma_predictive_entropy_mean(const float *neg_log_probs_per_token, int seq_len)
{
    if (!neg_log_probs_per_token || seq_len <= 0)
        return 0.f;
    double s = 0.0;
    for (int t = 0; t < seq_len; t++)
        s += (double)neg_log_probs_per_token[t];
    return (float)(s / (double)seq_len);
}

float sigma_semantic_entropy_cluster_equal(const int *const *top1_token_ids, int n_samples, int seq_len)
{
    if (!top1_token_ids || n_samples <= 0 || seq_len <= 0)
        return 0.f;
    int *cluster_id = (int *)calloc((size_t)n_samples, sizeof(int));
    if (!cluster_id)
        return 0.f;
    int n_clusters = 0;
    for (int s = 0; s < n_samples; s++) {
        cluster_id[s] = -1;
    }
    for (int s = 0; s < n_samples; s++) {
        if (cluster_id[s] >= 0)
            continue;
        cluster_id[s] = n_clusters;
        const int *row_s = top1_token_ids[s];
        for (int t = s + 1; t < n_samples; t++) {
            if (cluster_id[t] >= 0)
                continue;
            const int *row_t = top1_token_ids[t];
            if (!memcmp(row_s, row_t, sizeof(int) * (size_t)seq_len))
                cluster_id[t] = n_clusters;
        }
        n_clusters++;
    }
    int *counts = (int *)calloc((size_t)n_clusters, sizeof(int));
    if (!counts) {
        free(cluster_id);
        return 0.f;
    }
    for (int s = 0; s < n_samples; s++) {
        int c = cluster_id[s];
        if (c >= 0 && c < n_clusters)
            counts[c]++;
    }
    double ent = 0.0;
    for (int c = 0; c < n_clusters; c++) {
        double p = (double)counts[c] / (double)n_samples;
        if (p > 1e-12)
            ent -= p * log(p);
    }
    free(counts);
    free(cluster_id);
    double denom = log((double)n_clusters > 1 ? (double)n_clusters : 2.0);
    if (!(denom > 0.0))
        return 0.f;
    return (float)(ent / denom);
}

float sigma_critical_token_entropy_weighted(
    const float *per_token_entropy, const unsigned char *critical_mask, int n_tokens)
{
    if (!per_token_entropy || !critical_mask || n_tokens <= 0)
        return 0.f;
    double wsum = 0.0, w = 0.0;
    for (int i = 0; i < n_tokens; i++) {
        if (!critical_mask[i])
            continue;
        wsum += (double)per_token_entropy[i];
        w += 1.0;
    }
    if (w <= 0.0)
        return 0.f;
    return (float)(wsum / w);
}

float sigma_eigenscore_hidden_proxy(const float *hidden, int dim)
{
    if (!hidden || dim <= 0)
        return 0.f;
    double mean = 0.0, m2 = 0.0;
    for (int i = 0; i < dim; i++)
        mean += (double)hidden[i];
    mean /= (double)dim;
    for (int i = 0; i < dim; i++) {
        double d = (double)hidden[i] - mean;
        m2 += d * d;
    }
    double var = m2 / (double)dim;
    return (float)(1.0 - 1.0 / (1.0 + sqrt(var + 1e-12)));
}

void sigma_channels_v34_fill_last_step(const float *logits, int n_vocab, sigma_channels_v34_t *out)
{
    if (!out)
        return;
    memset(out, 0, sizeof(*out));
    if (!logits || n_vocab <= 0)
        return;
    sigma_decomposed_t d;
    sigma_decompose_dirichlet_evidence(logits, n_vocab, &d);
    out->logtoku_aleatoric = d.aleatoric;
    out->logtoku_epistemic = d.epistemic;
    out->logtoku_total = d.total;
    out->top_margin = sigma_top_margin(logits, n_vocab);
}
