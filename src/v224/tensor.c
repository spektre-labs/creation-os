/*
 * v224 σ-Tensor — reference implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "tensor.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static uint64_t fnv1a(const void *data, size_t n, uint64_t seed) {
    const uint8_t *p = (const uint8_t *)data;
    uint64_t h = seed ? seed : 0xcbf29ce484222325ULL;
    for (size_t i = 0; i < n; ++i) { h ^= p[i]; h *= 0x100000001b3ULL; }
    return h;
}

void cos_v224_init(cos_v224_state_t *s, uint64_t seed) {
    memset(s, 0, sizeof(*s));
    s->seed       = seed ? seed : 0x224750C1ULL;
    s->tau_rel    = 0.15f;
    s->delta_corr = 0.50f;
    s->rank       = COS_V224_RANK;
}

/* Build a rank-3 tensor with:
 *  - two latent channel groups (0..3 co-vary positively,
 *    4..7 co-vary positively), so the 8x8 correlation
 *    matrix is approximately block-structured rank-4.
 *  - deterministic per-token / per-context phase so the
 *    tensor is not constant.
 */
static float sig(float x) { return 1.0f / (1.0f + expf(-x)); }

void cos_v224_build(cos_v224_state_t *s) {
    /* Latent loadings for each channel: first 4 channels
     * load on latent a; last 4 load on latent b. */
    static const float LOAD_A[COS_V224_N_CHANNEL] =
        { 0.95f, 0.90f, 0.85f, 0.80f, 0.10f, 0.05f, 0.08f, 0.12f };
    static const float LOAD_B[COS_V224_N_CHANNEL] =
        { 0.10f, 0.15f, 0.12f, 0.08f, 0.95f, 0.90f, 0.85f, 0.80f };

    for (int t = 0; t < COS_V224_N_TOKENS; ++t) {
        float phase_t = 0.30f * (float)t;
        for (int k = 0; k < COS_V224_N_CONTEXT; ++k) {
            /* Two latent signals deterministic in (t,k). */
            float a = sinf(phase_t + 0.70f * (float)k) * 0.5f + 0.5f;
            float b = cosf(phase_t * 1.3f + 0.55f * (float)k) * 0.5f + 0.5f;
            for (int c = 0; c < COS_V224_N_CHANNEL; ++c) {
                float v = LOAD_A[c] * a + LOAD_B[c] * b;
                /* Squash into [0,1] with sigmoid so the
                 * tensor behaves like σ values. */
                v = sig(2.0f * (v - 0.5f));
                if (v < 0.0f) v = 0.0f;
                if (v > 1.0f) v = 1.0f;
                s->tensor[t][c][k] = v;
            }
        }
    }
    /* Channel weights: sum to 1, biased toward the
     * "block A" channels in this fixture. */
    float W[COS_V224_N_CHANNEL] =
        { 0.20f, 0.15f, 0.15f, 0.10f, 0.10f, 0.10f, 0.10f, 0.10f };
    float sum = 0.0f;
    for (int c = 0; c < COS_V224_N_CHANNEL; ++c) sum += W[c];
    if (sum <= 0.0f) sum = 1.0f;
    for (int c = 0; c < COS_V224_N_CHANNEL; ++c)
        s->weight_channel[c] = W[c] / sum;
}

/* Power-iteration-based rank-k approximation of a
 * symmetric matrix (R^T R).  Deflation after each
 * eigenvector.  Works on small matrices (C=8). */
static void sym_rank_k(const float R[COS_V224_N_CHANNEL][COS_V224_N_CHANNEL],
                       float out[COS_V224_N_CHANNEL][COS_V224_N_CHANNEL],
                       int k_rank) {
    float A[COS_V224_N_CHANNEL][COS_V224_N_CHANNEL];
    memcpy(A, R, sizeof(A));
    memset(out, 0, sizeof(float)*COS_V224_N_CHANNEL*COS_V224_N_CHANNEL);

    for (int r = 0; r < k_rank; ++r) {
        float v[COS_V224_N_CHANNEL];
        for (int i = 0; i < COS_V224_N_CHANNEL; ++i)
            v[i] = (i == (r % COS_V224_N_CHANNEL)) ? 1.0f : 0.5f;
        /* Deterministic power iteration. */
        for (int iter = 0; iter < 40; ++iter) {
            float w[COS_V224_N_CHANNEL] = {0};
            for (int i = 0; i < COS_V224_N_CHANNEL; ++i)
                for (int j = 0; j < COS_V224_N_CHANNEL; ++j)
                    w[i] += A[i][j] * v[j];
            float nrm = 0.0f;
            for (int i = 0; i < COS_V224_N_CHANNEL; ++i) nrm += w[i]*w[i];
            nrm = sqrtf(nrm);
            if (nrm < 1e-12f) break;
            for (int i = 0; i < COS_V224_N_CHANNEL; ++i) v[i] = w[i] / nrm;
        }
        /* Rayleigh quotient → eigenvalue. */
        float Av[COS_V224_N_CHANNEL] = {0};
        for (int i = 0; i < COS_V224_N_CHANNEL; ++i)
            for (int j = 0; j < COS_V224_N_CHANNEL; ++j)
                Av[i] += A[i][j] * v[j];
        float lam = 0.0f;
        for (int i = 0; i < COS_V224_N_CHANNEL; ++i) lam += v[i] * Av[i];

        /* Accumulate rank-1 outer product. */
        for (int i = 0; i < COS_V224_N_CHANNEL; ++i)
            for (int j = 0; j < COS_V224_N_CHANNEL; ++j)
                out[i][j] += lam * v[i] * v[j];
        /* Deflate. */
        for (int i = 0; i < COS_V224_N_CHANNEL; ++i)
            for (int j = 0; j < COS_V224_N_CHANNEL; ++j)
                A[i][j] -= lam * v[i] * v[j];
    }
}

void cos_v224_run(cos_v224_state_t *s) {
    /* 1. σ_agg[t] = Σ_c Σ_k w_c · T[t,c,k] / K */
    for (int t = 0; t < COS_V224_N_TOKENS; ++t) {
        float acc = 0.0f;
        for (int c = 0; c < COS_V224_N_CHANNEL; ++c) {
            float ksum = 0.0f;
            for (int k = 0; k < COS_V224_N_CONTEXT; ++k)
                ksum += s->tensor[t][c][k];
            acc += s->weight_channel[c] * (ksum / (float)COS_V224_N_CONTEXT);
        }
        if (acc < 0.0f) acc = 0.0f;
        if (acc > 1.0f) acc = 1.0f;
        s->sigma_agg[t] = acc;
    }
    /* 2. σ_gmean[t] = ( Π_c mean_k T[t,c,k] )^(1/C)
     *    — v29 geometric mean over channel means.
     *    Small floor ε to avoid log(0). */
    for (int t = 0; t < COS_V224_N_TOKENS; ++t) {
        float logsum = 0.0f;
        for (int c = 0; c < COS_V224_N_CHANNEL; ++c) {
            float ksum = 0.0f;
            for (int k = 0; k < COS_V224_N_CONTEXT; ++k)
                ksum += s->tensor[t][c][k];
            float m = ksum / (float)COS_V224_N_CONTEXT;
            if (m < 1e-6f) m = 1e-6f;
            logsum += logf(m);
        }
        float g = expf(logsum / (float)COS_V224_N_CHANNEL);
        if (g < 0.0f) g = 0.0f;
        if (g > 1.0f) g = 1.0f;
        s->sigma_gmean[t] = g;
    }
    /* 3. Channel correlation matrix across (t,k) samples,
     *    centred. */
    float mean[COS_V224_N_CHANNEL] = {0};
    int   n_samp = COS_V224_N_TOKENS * COS_V224_N_CONTEXT;
    for (int c = 0; c < COS_V224_N_CHANNEL; ++c) {
        float s_ = 0.0f;
        for (int t = 0; t < COS_V224_N_TOKENS; ++t)
            for (int k = 0; k < COS_V224_N_CONTEXT; ++k)
                s_ += s->tensor[t][c][k];
        mean[c] = s_ / (float)n_samp;
    }
    float var[COS_V224_N_CHANNEL] = {0};
    for (int c = 0; c < COS_V224_N_CHANNEL; ++c) {
        float s_ = 0.0f;
        for (int t = 0; t < COS_V224_N_TOKENS; ++t)
            for (int k = 0; k < COS_V224_N_CONTEXT; ++k) {
                float d = s->tensor[t][c][k] - mean[c];
                s_ += d*d;
            }
        var[c] = sqrtf(s_ / (float)n_samp);
        if (var[c] < 1e-6f) var[c] = 1e-6f;
    }
    for (int a = 0; a < COS_V224_N_CHANNEL; ++a) {
        for (int b = 0; b < COS_V224_N_CHANNEL; ++b) {
            float s_ = 0.0f;
            for (int t = 0; t < COS_V224_N_TOKENS; ++t)
                for (int k = 0; k < COS_V224_N_CONTEXT; ++k) {
                    float da = s->tensor[t][a][k] - mean[a];
                    float db = s->tensor[t][b][k] - mean[b];
                    s_ += da * db;
                }
            s->corr_full[a][b] = (s_ / (float)n_samp) / (var[a] * var[b]);
        }
    }
    /* 4. Rank-k approximation via symmetric power-iter. */
    sym_rank_k(s->corr_full, s->corr_lowrank, s->rank);

    /* 5. Relative Frobenius reconstruction error. */
    float num = 0.0f, den = 0.0f;
    for (int i = 0; i < COS_V224_N_CHANNEL; ++i)
        for (int j = 0; j < COS_V224_N_CHANNEL; ++j) {
            float d = s->corr_full[i][j] - s->corr_lowrank[i][j];
            num += d*d;
            den += s->corr_full[i][j] * s->corr_full[i][j];
        }
    s->rel_err = (den > 0.0f) ? sqrtf(num / den) : 0.0f;
    s->params_full    = COS_V224_N_CHANNEL * COS_V224_N_CHANNEL;
    /* Low-rank symmetric storage: k_rank eigenvectors
     * (C floats each) + k_rank eigenvalues.  The off-
     * diagonal half is recovered by symmetry — that is
     * the compression. */
    s->params_lowrank = s->rank * (COS_V224_N_CHANNEL + 1);

    /* 6. Divergence count: σ_agg vs σ_gmean. */
    s->n_divergent = 0;
    for (int t = 0; t < COS_V224_N_TOKENS; ++t)
        if (fabsf(s->sigma_agg[t] - s->sigma_gmean[t]) > 1e-4f)
            s->n_divergent++;

    /* 7. FNV-1a chain over tensor + weights + outputs. */
    uint64_t prev = 0x224D11A1ULL;
    for (int t = 0; t < COS_V224_N_TOKENS; ++t)
        for (int c = 0; c < COS_V224_N_CHANNEL; ++c)
            for (int k = 0; k < COS_V224_N_CONTEXT; ++k) {
                struct { int t, c, k; float v; uint64_t prev; } rec;
                memset(&rec, 0, sizeof(rec));
                rec.t = t; rec.c = c; rec.k = k;
                rec.v = s->tensor[t][c][k];
                rec.prev = prev;
                prev = fnv1a(&rec, sizeof(rec), prev);
            }
    for (int c = 0; c < COS_V224_N_CHANNEL; ++c) {
        struct { int c; float w; uint64_t prev; } rec;
        memset(&rec, 0, sizeof(rec));
        rec.c = c; rec.w = s->weight_channel[c]; rec.prev = prev;
        prev = fnv1a(&rec, sizeof(rec), prev);
    }
    for (int t = 0; t < COS_V224_N_TOKENS; ++t) {
        struct { int t; float sa, sg; uint64_t prev; } rec;
        memset(&rec, 0, sizeof(rec));
        rec.t  = t;
        rec.sa = s->sigma_agg[t];
        rec.sg = s->sigma_gmean[t];
        rec.prev = prev;
        prev = fnv1a(&rec, sizeof(rec), prev);
    }
    {
        struct { float re; int pf, pl, nd; uint64_t prev; } rec;
        memset(&rec, 0, sizeof(rec));
        rec.re = s->rel_err;
        rec.pf = s->params_full;
        rec.pl = s->params_lowrank;
        rec.nd = s->n_divergent;
        rec.prev = prev;
        prev = fnv1a(&rec, sizeof(rec), prev);
    }
    s->terminal_hash = prev;
    s->chain_valid   = true;
}

size_t cos_v224_to_json(const cos_v224_state_t *s, char *buf, size_t cap) {
    int n = snprintf(buf, cap,
        "{\"kernel\":\"v224\","
        "\"n_tokens\":%d,\"n_channel\":%d,\"n_context\":%d,\"rank\":%d,"
        "\"tau_rel\":%.3f,\"delta_corr\":%.3f,"
        "\"rel_err\":%.4f,\"params_full\":%d,\"params_lowrank\":%d,"
        "\"n_divergent\":%d,"
        "\"chain_valid\":%s,\"terminal_hash\":\"%016llx\","
        "\"weight_channel\":[",
        COS_V224_N_TOKENS, COS_V224_N_CHANNEL, COS_V224_N_CONTEXT, s->rank,
        s->tau_rel, s->delta_corr,
        s->rel_err, s->params_full, s->params_lowrank,
        s->n_divergent,
        s->chain_valid ? "true" : "false",
        (unsigned long long)s->terminal_hash);
    if (n < 0 || (size_t)n >= cap) return 0;
    size_t off = (size_t)n;
    for (int c = 0; c < COS_V224_N_CHANNEL; ++c) {
        int k = snprintf(buf + off, cap - off,
            "%s%.4f", c == 0 ? "" : ",", s->weight_channel[c]);
        if (k < 0 || off + (size_t)k >= cap) return 0;
        off += (size_t)k;
    }
    int q = snprintf(buf + off, cap - off, "],\"sigma_agg\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int t = 0; t < COS_V224_N_TOKENS; ++t) {
        int k = snprintf(buf + off, cap - off,
            "%s%.4f", t == 0 ? "" : ",", s->sigma_agg[t]);
        if (k < 0 || off + (size_t)k >= cap) return 0;
        off += (size_t)k;
    }
    int q2 = snprintf(buf + off, cap - off, "],\"sigma_gmean\":[");
    if (q2 < 0 || off + (size_t)q2 >= cap) return 0;
    off += (size_t)q2;
    for (int t = 0; t < COS_V224_N_TOKENS; ++t) {
        int k = snprintf(buf + off, cap - off,
            "%s%.4f", t == 0 ? "" : ",", s->sigma_gmean[t]);
        if (k < 0 || off + (size_t)k >= cap) return 0;
        off += (size_t)k;
    }
    int m = snprintf(buf + off, cap - off, "]}");
    if (m < 0 || off + (size_t)m >= cap) return 0;
    return off + (size_t)m;
}

int cos_v224_self_test(void) {
    cos_v224_state_t s;
    cos_v224_init(&s, 0x224750C1ULL);
    cos_v224_build(&s);
    cos_v224_run(&s);

    if (!s.chain_valid) return 1;

    float wsum = 0.0f;
    for (int c = 0; c < COS_V224_N_CHANNEL; ++c) {
        if (s.weight_channel[c] < 0.0f ||
            s.weight_channel[c] > 1.0f) return 2;
        wsum += s.weight_channel[c];
    }
    if (fabsf(wsum - 1.0f) > 1e-5f) return 2;

    for (int t = 0; t < COS_V224_N_TOKENS; ++t) {
        for (int c = 0; c < COS_V224_N_CHANNEL; ++c)
            for (int k = 0; k < COS_V224_N_CONTEXT; ++k)
                if (s.tensor[t][c][k] < 0.0f ||
                    s.tensor[t][c][k] > 1.0f) return 3;
        if (s.sigma_agg[t]   < 0.0f || s.sigma_agg[t]   > 1.0f) return 4;
        if (s.sigma_gmean[t] < 0.0f || s.sigma_gmean[t] > 1.0f) return 4;
        if (fabsf(s.sigma_agg[t] - s.sigma_gmean[t]) > s.delta_corr) return 5;
    }

    if (s.rel_err > s.tau_rel) return 6;
    if (s.params_lowrank >= s.params_full) return 7;
    if (s.n_divergent < 1) return 8;
    return 0;
}
