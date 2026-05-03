/* SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only */
/*
 * v157 σ-attention inference (reference C).
 * Sub-quadratic pattern: full attention only for low-σ query rows; high-σ rows use a window.
 */
#include "sigma_attention.h"

#include <string.h>

static int32_t dot_prod(const int32_t *a, const int32_t *b, int32_t d)
{
    int64_t s;
    int32_t k;

    s = 0;
    for (k = 0; k < d; k++)
        s += (int64_t)a[k] * (int64_t)b[k];
    if (s > 2147483647LL)
        return 2147483647;
    if (s < -2147483647LL - 1LL)
        return -2147483647 - 1;
    return (int32_t)s;
}

static void swap_kv_row(int32_t *kv_cache, int32_t *sigmas, int32_t head_dim, int32_t a,
                        int32_t b)
{
    int32_t      stride;
    int32_t      i;
    int32_t      t;
    int32_t     *pa;
    int32_t     *pb;

    stride = head_dim * 2;
    pa     = kv_cache + (int32_t)(a * stride);
    pb     = kv_cache + (int32_t)(b * stride);
    for (i = 0; i < stride; i++) {
        t      = pa[i];
        pa[i]  = pb[i];
        pb[i]  = t;
    }
    t           = sigmas[a];
    sigmas[a]   = sigmas[b];
    sigmas[b]   = t;
}

static void one_row_attend(const int32_t *q_row, const int32_t *K, const int32_t *V,
                           int32_t d, int32_t j_start, int32_t j_end, int32_t *out_row)
{
    int64_t acc[COS_INFER_ATTN_DIM_MAX];
    int64_t sumw;
    int32_t j;
    int32_t k;

    if (d > COS_INFER_ATTN_DIM_MAX)
        return;

    for (k = 0; k < d; k++)
        acc[k]  = 0;
    sumw = 0;

    for (j = j_start; j < j_end; j++) {
        const int32_t *kj;
        const int32_t *vj;
        int32_t        s;
        int64_t        ws;

        kj = K + j * d;
        vj = V + j * d;
        s  = dot_prod(q_row, kj, d);
        if (s < 0)
            s = 0;
        ws = (int64_t)s + 1LL;
        sumw += ws;
        for (k = 0; k < d; k++)
            acc[k] += ws * (int64_t)vj[k];
    }

    if (sumw <= 0LL) {
        for (k = 0; k < d; k++)
            out_row[k] = 0;
        return;
    }

    for (k = 0; k < d; k++) {
        int64_t q;
        q = acc[k] / sumw;
        if (q > 2147483647LL)
            q = 2147483647LL;
        if (q < -2147483647LL - 1LL)
            q = -2147483647LL - 1LL;
        out_row[k] = (int32_t)q;
    }
}

void cos_infer_sigma_attention_init(cos_infer_sigma_attention_t *sa, int32_t seq_len,
                                    int32_t head_dim, int32_t n_heads, int32_t *token_sigmas,
                                    int32_t sigma_threshold_q16)
{
    if (!sa)
        return;
    memset(sa, 0, sizeof(*sa));
    sa->seq_len             = seq_len;
    sa->head_dim            = head_dim;
    sa->n_heads             = n_heads > 0 ? n_heads : 1;
    sa->token_sigmas        = token_sigmas;
    sa->sigma_threshold_q16 = sigma_threshold_q16;
}

void cos_infer_sigma_attention_reset_stats(cos_infer_sigma_attention_t *sa)
{
    if (!sa)
        return;
    sa->tokens_full   = 0U;
    sa->tokens_pruned = 0U;
}

void cos_infer_sigma_sparse_attention(cos_infer_sigma_attention_t *sa, const int32_t *Q,
                                      const int32_t *K, const int32_t *V, int32_t *output,
                                      int32_t window_size)
{
    int32_t n;
    int32_t d;
    int32_t i;
    int32_t w;

    if (!sa || !Q || !K || !V || !output || !sa->token_sigmas)
        return;
    n = sa->seq_len;
    d = sa->head_dim;
    if (d > COS_INFER_ATTN_DIM_MAX || n <= 0 || d <= 0)
        return;
    w = window_size > 0 ? window_size : 1;

    sa->tokens_full   = 0U;
    sa->tokens_pruned = 0U;

    for (i = 0; i < n; i++) {
        const int32_t *qi;
        int32_t       *oi;

        qi = Q + i * d;
        oi = output + i * d;

        if (sa->token_sigmas[i] < sa->sigma_threshold_q16) {
            one_row_attend(qi, K, V, d, 0, n, oi);
            sa->tokens_full++;
        } else {
            int32_t lo;
            int32_t hi;

            lo = i - w;
            hi = i + w + 1;
            if (lo < 0)
                lo = 0;
            if (hi > n)
                hi = n;
            one_row_attend(qi, K, V, d, lo, hi, oi);
            sa->tokens_pruned++;
        }
    }
}

int32_t cos_infer_sigma_kv_prune(cos_infer_sigma_attention_t *sa, int32_t *kv_cache,
                                 int32_t *sigmas, int32_t cache_len, int32_t head_dim,
                                 int32_t max_cache)
{
    int32_t worst_idx;
    int32_t worst_sig;
    int32_t i;
    int32_t new_len;

    if (!sa || !kv_cache || !sigmas || head_dim <= 0)
        return cache_len;
    if (cache_len <= max_cache)
        return cache_len;

    worst_idx = 0;
    worst_sig = sigmas[0];
    for (i = 1; i < cache_len; i++) {
        if (sigmas[i] > worst_sig) {
            worst_sig = sigmas[i];
            worst_idx = i;
        }
    }

    new_len = cache_len - 1;
    if (worst_idx != new_len)
        swap_kv_row(kv_cache, sigmas, head_dim, worst_idx, new_len);
    sa->tokens_pruned++;
    return new_len;
}

void cos_infer_sigma_attention_ops_estimate(const cos_infer_sigma_attention_t *sa,
                                            int32_t seq_len, int32_t window_size, int32_t head_dim,
                                            int64_t *out_sparse_ops, int64_t *out_dense_ops)
{
    int64_t dense;
    int64_t sparse;
    uint32_t f;
    uint32_t p;
    int32_t w;
    int32_t span;

    if (!out_sparse_ops || !out_dense_ops)
        return;

    dense = (int64_t)seq_len * (int64_t)seq_len * (int64_t)head_dim;
    *out_dense_ops = dense;

    if (!sa) {
        *out_sparse_ops = dense;
        return;
    }

    f = sa->tokens_full;
    p = sa->tokens_pruned;
    w = window_size > 0 ? window_size : 1;
    span = w * 2 + 1;
    if (span > seq_len)
        span = seq_len;

    sparse = (int64_t)f * (int64_t)seq_len * (int64_t)head_dim
             + (int64_t)p * (int64_t)span * (int64_t)head_dim;
    *out_sparse_ops = sparse;
}
