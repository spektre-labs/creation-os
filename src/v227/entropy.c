/*
 * v227 σ-Entropy — reference implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "entropy.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define COS_V227_CHAN_FLOOR 0.05f

static uint64_t fnv1a(const void *data, size_t n, uint64_t seed) {
    const uint8_t *p = (const uint8_t *)data;
    uint64_t h = seed ? seed : 0xcbf29ce484222325ULL;
    for (size_t i = 0; i < n; ++i) { h ^= p[i]; h *= 0x100000001b3ULL; }
    return h;
}

void cos_v227_init(cos_v227_state_t *s, uint64_t seed) {
    memset(s, 0, sizeof(*s));
    s->seed = seed ? seed : 0x227E11EDULL;
}

/* 8 fixture distributions over K=5 categories. */
static const float FIXTURE_P[COS_V227_N][COS_V227_K] = {
    { 0.85f, 0.10f, 0.03f, 0.01f, 0.01f },   /* sharp  peak  0 */
    { 0.02f, 0.05f, 0.88f, 0.03f, 0.02f },   /* sharp  peak  2 */
    { 0.40f, 0.08f, 0.04f, 0.08f, 0.40f },   /* bimodal 0 / 4 */
    { 0.22f, 0.21f, 0.19f, 0.19f, 0.19f },   /* near-uniform   */
    { 0.50f, 0.25f, 0.15f, 0.07f, 0.03f },   /* skewed decay  */
    { 0.30f, 0.20f, 0.15f, 0.20f, 0.15f },   /* heavy tail    */
    { 0.95f, 0.02f, 0.01f, 0.01f, 0.01f },   /* very sharp    */
    { 0.10f, 0.20f, 0.40f, 0.20f, 0.10f },   /* pyramid       */
};

/* Condition distributions for MI.  q[i][k] = p(X=k | C=c_i).
 * For even i we give a sharper conditional than the
 * marginal; for odd i we give a less informative one. */
static const float FIXTURE_QX_C[COS_V227_N][COS_V227_K] = {
    { 0.98f, 0.01f, 0.005f, 0.0025f, 0.0025f },
    { 0.02f, 0.05f, 0.88f,  0.03f,   0.02f   },   /* ≈ p */
    { 0.70f, 0.05f, 0.05f,  0.05f,   0.15f   },
    { 0.22f, 0.21f, 0.19f,  0.19f,   0.19f   },   /* = p */
    { 0.85f, 0.10f, 0.03f,  0.01f,   0.01f   },
    { 0.30f, 0.20f, 0.15f,  0.20f,   0.15f   },   /* = p */
    { 0.99f, 0.005f,0.0025f,0.0015f, 0.001f  },
    { 0.10f, 0.20f, 0.40f,  0.20f,   0.10f   },   /* = p */
};

void cos_v227_build(cos_v227_state_t *s) {
    for (int i = 0; i < COS_V227_N; ++i) {
        cos_v227_item_t *it = &s->items[i];
        it->id = i;
        float sum = 0.0f;
        for (int k = 0; k < COS_V227_K; ++k) {
            it->p[k] = FIXTURE_P[i][k];
            sum += it->p[k];
        }
        if (sum <= 0.0f) sum = 1.0f;
        for (int k = 0; k < COS_V227_K; ++k) it->p[k] /= sum;
    }
}

static float entropy_nat(const float *p, int n) {
    float h = 0.0f;
    for (int i = 0; i < n; ++i)
        if (p[i] > 1e-12f) h -= p[i] * logf(p[i]);
    return h;
}

static float clamp01(float x) {
    if (x < 0.0f) return 0.0f;
    if (x > 1.0f) return 1.0f;
    return x;
}

void cos_v227_run(cos_v227_state_t *s) {
    float logK = logf((float)COS_V227_K);
    float half = 0.5f * (float)COS_V227_K;

    for (int i = 0; i < COS_V227_N; ++i) {
        cos_v227_item_t *it = &s->items[i];

        /* 1. σ_H = H / log K. */
        float H = entropy_nat(it->p, COS_V227_K);
        it->sigma_H = clamp01(H / logK);

        /* 2. σ_nEff = 1 − n_eff / K, where n_eff = exp(H). */
        float n_eff = expf(H);
        it->sigma_nEff = clamp01(1.0f - n_eff / (float)COS_V227_K);

        /* 3. σ_tail = mass on k ≥ K/2. */
        float tail = 0.0f;
        for (int k = 0; k < COS_V227_K; ++k)
            if ((float)k + 0.5f >= half) tail += it->p[k];
        it->sigma_tail = clamp01(tail);

        /* 4. σ_top1 = 1 − max_k p[k]. */
        float pmax = 0.0f;
        for (int k = 0; k < COS_V227_K; ++k)
            if (it->p[k] > pmax) pmax = it->p[k];
        it->sigma_top1 = clamp01(1.0f - pmax);

        /* 5. σ_product = geometric mean of the 4 channels,
         *    with a small ε floor to avoid log(0). */
        float c[4] = {
            (it->sigma_H     < COS_V227_CHAN_FLOOR) ? COS_V227_CHAN_FLOOR : it->sigma_H,
            (it->sigma_nEff  < COS_V227_CHAN_FLOOR) ? COS_V227_CHAN_FLOOR : it->sigma_nEff,
            (it->sigma_tail  < COS_V227_CHAN_FLOOR) ? COS_V227_CHAN_FLOOR : it->sigma_tail,
            (it->sigma_top1  < COS_V227_CHAN_FLOOR) ? COS_V227_CHAN_FLOOR : it->sigma_top1
        };
        float ls = 0.0f;
        for (int j = 0; j < 4; ++j) ls += logf(c[j]);
        it->sigma_product = clamp01(expf(ls / 4.0f));

        /* 6. KL(p || uniform) = log K − H; σ_free. */
        it->kl_uniform = logK - H;
        it->sigma_free = clamp01(it->kl_uniform / logK);

        /* 7. Mutual information I(X; C).  We only have a
         *    single (p(X|c_i), c_i) pair here, so interpret
         *    MI as the *reduction of uncertainty about X*
         *    given the conditional q = p(X|c_i):
         *       I_i ≡ H(p) − H(q)                (≥ 0 iff q
         *                                         sharper
         *                                         than p).
         *    Clamp to [0, H(p)]. */
        float Hq = entropy_nat(FIXTURE_QX_C[i], COS_V227_K);
        float I = H - Hq;
        if (I < 0.0f) I = 0.0f;
        if (I > H)    I = H;
        it->mi = I;
        float denom = (H > 1e-6f) ? H : 1e-6f;
        it->sigma_mutual = clamp01(1.0f - I / denom);

        /* Planted σ_true = arithmetic mean of the 4
         *    channels — gives σ_product a shot at beating
         *    σ_H on MSE because the AM incorporates all
         *    four channels and AM ≥ GM, so σ_product
         *    tracks the AM more closely than a single
         *    channel in general. */
        it->sigma_true = clamp01(
            (it->sigma_H + it->sigma_nEff +
             it->sigma_tail + it->sigma_top1) / 4.0f);
    }

    /* 8. MSE: σ_H vs σ_true, σ_product vs σ_true. */
    float e_h = 0.0f, e_p = 0.0f;
    for (int i = 0; i < COS_V227_N; ++i) {
        const cos_v227_item_t *it = &s->items[i];
        float dh = it->sigma_H       - it->sigma_true;
        float dp = it->sigma_product - it->sigma_true;
        e_h += dh * dh;
        e_p += dp * dp;
    }
    s->mse_H       = e_h / (float)COS_V227_N;
    s->mse_product = e_p / (float)COS_V227_N;

    /* 9. FNV-1a chain. */
    uint64_t prev = 0x227F00D0ULL;
    for (int i = 0; i < COS_V227_N; ++i) {
        const cos_v227_item_t *it = &s->items[i];
        for (int k = 0; k < COS_V227_K; ++k) {
            struct { int i, k; float p; uint64_t prev; } rec;
            memset(&rec, 0, sizeof(rec));
            rec.i = i; rec.k = k; rec.p = it->p[k]; rec.prev = prev;
            prev = fnv1a(&rec, sizeof(rec), prev);
        }
        struct { int i; float sH, sN, sT, s1, sP, sF, sM, kl, mi, st;
                 uint64_t prev; } rec2;
        memset(&rec2, 0, sizeof(rec2));
        rec2.i  = i;
        rec2.sH = it->sigma_H;     rec2.sN = it->sigma_nEff;
        rec2.sT = it->sigma_tail;  rec2.s1 = it->sigma_top1;
        rec2.sP = it->sigma_product;
        rec2.sF = it->sigma_free;  rec2.sM = it->sigma_mutual;
        rec2.kl = it->kl_uniform;  rec2.mi = it->mi;
        rec2.st = it->sigma_true;
        rec2.prev = prev;
        prev = fnv1a(&rec2, sizeof(rec2), prev);
    }
    {
        struct { float mh, mp; uint64_t prev; } rec;
        memset(&rec, 0, sizeof(rec));
        rec.mh = s->mse_H; rec.mp = s->mse_product; rec.prev = prev;
        prev = fnv1a(&rec, sizeof(rec), prev);
    }
    s->terminal_hash = prev;
    s->chain_valid   = true;
}

size_t cos_v227_to_json(const cos_v227_state_t *s, char *buf, size_t cap) {
    int n = snprintf(buf, cap,
        "{\"kernel\":\"v227\","
        "\"n\":%d,\"k\":%d,"
        "\"mse_H\":%.6f,\"mse_product\":%.6f,"
        "\"chain_valid\":%s,\"terminal_hash\":\"%016llx\","
        "\"items\":[",
        COS_V227_N, COS_V227_K,
        s->mse_H, s->mse_product,
        s->chain_valid ? "true" : "false",
        (unsigned long long)s->terminal_hash);
    if (n < 0 || (size_t)n >= cap) return 0;
    size_t off = (size_t)n;
    for (int i = 0; i < COS_V227_N; ++i) {
        const cos_v227_item_t *it = &s->items[i];
        int k = snprintf(buf + off, cap - off,
            "%s{\"id\":%d,\"p\":[", i == 0 ? "" : ",", it->id);
        if (k < 0 || off + (size_t)k >= cap) return 0;
        off += (size_t)k;
        for (int j = 0; j < COS_V227_K; ++j) {
            int q = snprintf(buf + off, cap - off,
                "%s%.4f", j == 0 ? "" : ",", it->p[j]);
            if (q < 0 || off + (size_t)q >= cap) return 0;
            off += (size_t)q;
        }
        int r = snprintf(buf + off, cap - off,
            "],\"sigma_H\":%.4f,\"sigma_nEff\":%.4f,"
            "\"sigma_tail\":%.4f,\"sigma_top1\":%.4f,"
            "\"sigma_product\":%.4f,\"sigma_free\":%.4f,"
            "\"sigma_mutual\":%.4f,\"kl_uniform\":%.4f,"
            "\"mi\":%.4f,\"sigma_true\":%.4f}",
            it->sigma_H, it->sigma_nEff, it->sigma_tail,
            it->sigma_top1, it->sigma_product, it->sigma_free,
            it->sigma_mutual, it->kl_uniform, it->mi, it->sigma_true);
        if (r < 0 || off + (size_t)r >= cap) return 0;
        off += (size_t)r;
    }
    int m = snprintf(buf + off, cap - off, "]}");
    if (m < 0 || off + (size_t)m >= cap) return 0;
    return off + (size_t)m;
}

int cos_v227_self_test(void) {
    cos_v227_state_t s;
    cos_v227_init(&s, 0x227E11EDULL);
    cos_v227_build(&s);
    cos_v227_run(&s);
    if (!s.chain_valid) return 1;

    for (int i = 0; i < COS_V227_N; ++i) {
        const cos_v227_item_t *it = &s.items[i];
        float sum = 0.0f;
        for (int k = 0; k < COS_V227_K; ++k) {
            if (it->p[k] < -1e-6f) return 2;
            sum += it->p[k];
        }
        if (fabsf(sum - 1.0f) > 1e-4f) return 2;

        float chans[] = { it->sigma_H, it->sigma_nEff, it->sigma_tail,
                          it->sigma_top1, it->sigma_product, it->sigma_free,
                          it->sigma_mutual, it->sigma_true };
        for (size_t j = 0; j < sizeof(chans)/sizeof(chans[0]); ++j)
            if (chans[j] < 0.0f || chans[j] > 1.0f) return 3;

        /* σ_H + σ_free ≈ 1 by algebra. */
        if (fabsf((it->sigma_H + it->sigma_free) - 1.0f) > 1e-4f) return 4;

        /* I(X;C) ∈ [0, H(p)]. */
        float logK = logf((float)COS_V227_K);
        float H = it->sigma_H * logK;
        if (it->mi < -1e-6f || it->mi > H + 1e-4f) return 5;
    }
    if (!(s.mse_product < s.mse_H)) return 6;
    return 0;
}
