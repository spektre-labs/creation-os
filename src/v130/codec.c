/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (c) 2026 Spektre Labs / Lauri Rainio
 *
 * v130 σ-Codec — implementation.
 */
#include "codec.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ==================================================================
 * FP4 LoRA-delta codec.
 *
 * Layout per nibble:  [sign : 1][mantissa : 3]
 *   code = round(|x| / scale · 7)    in [0..7]
 *   bit7 = sign
 * Reconstruction: x ≈ sign · code · scale / 7
 *
 * On ~Gaussian LoRA deltas the round-trip rel-RMSE hovers around
 * 1/14 ≈ 0.07 — well within the acceptance band for preview /
 * transport.  v130.1 can swap this for a non-linear (E2M1-style)
 * encoding without touching the API.
 * ================================================================ */

static inline uint8_t fp4_encode_one(float x, float inv_scale) {
    if (!isfinite(x)) x = 0.0f;
    float ax = fabsf(x) * inv_scale;     /* in [0,1] */
    if (ax > 1.0f) ax = 1.0f;
    int   m  = (int)lrintf(ax * 7.0f);   /* 0..7 */
    if (m < 0) m = 0; if (m > 7) m = 7;
    uint8_t code = (uint8_t)m;
    if (x < 0.0f) code |= 0x8;
    return code;                           /* 0..15 */
}

static inline float fp4_decode_one(uint8_t code, float scale) {
    int sign = (code & 0x8) ? -1 : 1;
    int m    = code & 0x7;
    return (float)sign * ((float)m * scale / 7.0f);
}

float cos_v130_quantize_fp4(const float *in, int n, uint8_t *packed) {
    if (!in || !packed || n <= 0) return 0.0f;
    float scale = 0.0f;
    for (int i = 0; i < n; ++i) {
        float a = fabsf(in[i]);
        if (a > scale) scale = a;
    }
    if (scale <= 0.0f) scale = 1e-12f;   /* avoid div/0 */
    float inv = 1.0f / scale;

    int packed_n = (n + 1) / 2;
    memset(packed, 0, (size_t)packed_n);
    for (int i = 0; i < n; ++i) {
        uint8_t c = fp4_encode_one(in[i], inv);
        int byte = i >> 1;
        int hi   = i & 1;
        packed[byte] |= (uint8_t)(c << (hi ? 4 : 0));
    }
    return scale;
}

void cos_v130_dequantize_fp4(const uint8_t *packed, int n,
                             float scale, float *out) {
    if (!packed || !out || n <= 0) return;
    for (int i = 0; i < n; ++i) {
        int byte = i >> 1;
        int hi   = i & 1;
        uint8_t code = (packed[byte] >> (hi ? 4 : 0)) & 0x0F;
        out[i] = fp4_decode_one(code, scale);
    }
}

/* ==================================================================
 * σ-profile packer (8 floats in [0,1] ↔ 8 bytes).
 * ================================================================ */

void cos_v130_pack_sigma(const float *channels, uint8_t *out) {
    if (!channels || !out) return;
    for (int i = 0; i < COS_V130_SIGMA_DIM; ++i) {
        float c = channels[i];
        if (c < 0.0f) c = 0.0f;
        if (c > 1.0f) c = 1.0f;
        int v = (int)lrintf(c * 255.0f);
        if (v < 0) v = 0; if (v > 255) v = 255;
        out[i] = (uint8_t)v;
    }
}

void cos_v130_unpack_sigma(const uint8_t *codes, float *channels) {
    if (!codes || !channels) return;
    for (int i = 0; i < COS_V130_SIGMA_DIM; ++i)
        channels[i] = (float)codes[i] / 255.0f;
}

/* ==================================================================
 * Product Quantization.
 * ================================================================ */

static uint64_t splitmix64(uint64_t *s) {
    uint64_t x = (*s += 0x9E3779B97F4A7C15ULL);
    x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ULL;
    x = (x ^ (x >> 27)) * 0x94D049BB133111EBULL;
    return x ^ (x >> 31);
}

static inline float sub_dist2(const float *a, const float *b, int d) {
    float s = 0.0f;
    for (int i = 0; i < d; ++i) { float t = a[i] - b[i]; s += t*t; }
    return s;
}

int cos_v130_pq_train(cos_v130_pq_codebook_t *cb,
                      const float *vectors, int n_vectors,
                      int iters, uint64_t seed) {
    if (!cb || !vectors || n_vectors < COS_V130_PQ_CODES || iters < 1)
        return -1;
    memset(cb, 0, sizeof *cb);
    uint64_t rng = seed ? seed : 0xC0DECAFEULL;

    /* For each subspace, run mini-Lloyd. */
    for (int m = 0; m < COS_V130_PQ_SUBSPACES; ++m) {
        int off = m * COS_V130_PQ_SUB_DIM;

        /* Init K centroids by picking K pseudo-random training rows. */
        for (int k = 0; k < COS_V130_PQ_CODES; ++k) {
            uint64_t r = splitmix64(&rng);
            int src = (int)(r % (uint64_t)n_vectors);
            memcpy(cb->centroids[m][k],
                   vectors + (size_t)src * COS_V130_EMBED_DIM + off,
                   sizeof(float) * COS_V130_PQ_SUB_DIM);
        }

        int   *counts = (int*)  malloc(sizeof(int)   * COS_V130_PQ_CODES);
        float *accum  = (float*)malloc(sizeof(float) * COS_V130_PQ_CODES
                                                     * COS_V130_PQ_SUB_DIM);
        if (!counts || !accum) { free(counts); free(accum); return -1; }

        for (int it = 0; it < iters; ++it) {
            memset(counts, 0, sizeof(int) * COS_V130_PQ_CODES);
            memset(accum , 0, sizeof(float) * COS_V130_PQ_CODES
                                            * COS_V130_PQ_SUB_DIM);
            for (int i = 0; i < n_vectors; ++i) {
                const float *v = vectors + (size_t)i * COS_V130_EMBED_DIM + off;
                int   best = 0;
                float bd   = sub_dist2(v, cb->centroids[m][0],
                                       COS_V130_PQ_SUB_DIM);
                for (int k = 1; k < COS_V130_PQ_CODES; ++k) {
                    float d = sub_dist2(v, cb->centroids[m][k],
                                        COS_V130_PQ_SUB_DIM);
                    if (d < bd) { bd = d; best = k; }
                }
                counts[best]++;
                float *a = accum + (size_t)best * COS_V130_PQ_SUB_DIM;
                for (int j = 0; j < COS_V130_PQ_SUB_DIM; ++j) a[j] += v[j];
            }
            for (int k = 0; k < COS_V130_PQ_CODES; ++k) {
                if (counts[k] == 0) continue;
                float *a = accum + (size_t)k * COS_V130_PQ_SUB_DIM;
                for (int j = 0; j < COS_V130_PQ_SUB_DIM; ++j)
                    cb->centroids[m][k][j] = a[j] / (float)counts[k];
            }
        }
        free(counts); free(accum);
    }
    cb->trained = 1;
    return 0;
}

int cos_v130_pq_encode(const cos_v130_pq_codebook_t *cb,
                       const float *vec, uint8_t *out_codes) {
    if (!cb || !vec || !out_codes || !cb->trained) return -1;
    for (int m = 0; m < COS_V130_PQ_SUBSPACES; ++m) {
        const float *v = vec + m * COS_V130_PQ_SUB_DIM;
        int   best = 0;
        float bd   = sub_dist2(v, cb->centroids[m][0], COS_V130_PQ_SUB_DIM);
        for (int k = 1; k < COS_V130_PQ_CODES; ++k) {
            float d = sub_dist2(v, cb->centroids[m][k], COS_V130_PQ_SUB_DIM);
            if (d < bd) { bd = d; best = k; }
        }
        out_codes[m] = (uint8_t)best;
    }
    return 0;
}

int cos_v130_pq_decode(const cos_v130_pq_codebook_t *cb,
                       const uint8_t *codes, float *out_vec) {
    if (!cb || !codes || !out_vec || !cb->trained) return -1;
    for (int m = 0; m < COS_V130_PQ_SUBSPACES; ++m) {
        memcpy(out_vec + m * COS_V130_PQ_SUB_DIM,
               cb->centroids[m][codes[m]],
               sizeof(float) * COS_V130_PQ_SUB_DIM);
    }
    return 0;
}

/* ==================================================================
 * σ-aware context compression.
 * ================================================================ */

int cos_v130_compress_context(const cos_v130_chunk_t *chunks, int n,
                              size_t total_budget,
                              char *out, size_t cap) {
    if (!chunks || !out || n <= 0 || cap == 0) return -1;
    /* Weights w_i = 0.5 + σ_i (confident chunks compress more). */
    double wsum = 0.0;
    double *w = (double*)malloc(sizeof(double) * (size_t)n);
    if (!w) return -1;
    for (int i = 0; i < n; ++i) {
        float s = chunks[i].sigma_product;
        if (s < 0.0f) s = 0.0f; if (s > 1.0f) s = 1.0f;
        w[i] = 0.5 + (double)s;
        wsum += w[i];
    }
    if (wsum <= 0.0) { free(w); return -1; }
    /* Reserve room for n-1 separators " ... " (5 chars). */
    size_t overhead = (n > 1) ? (size_t)(n - 1) * 5 : 0;
    size_t budget = total_budget > overhead ? total_budget - overhead : 0;

    size_t off = 0;
    for (int i = 0; i < n; ++i) {
        size_t alloc = (size_t)((double)budget * w[i] / wsum);
        if (alloc < 8) alloc = 8;  /* minimum readable */
        const char *t = chunks[i].text;
        size_t tlen = strlen(t);
        size_t take = (tlen <= alloc) ? tlen : alloc;
        if (off + take + 5 >= cap) break;
        memcpy(out + off, t, take);
        off += take;
        if (take < tlen) {
            if (off + 3 < cap) { memcpy(out + off, "...", 3); off += 3; }
        }
        if (i + 1 < n) {
            if (off + 5 < cap) { memcpy(out + off, " ... ", 5); off += 5; }
        }
    }
    if (off < cap) out[off] = '\0';
    else out[cap - 1] = '\0';
    free(w);
    return (int)off;
}

/* ==================================================================
 * JSON helpers
 * ================================================================ */

int cos_v130_fp4_stats_to_json(const cos_v130_fp4_stats_t *s,
                               char *out, size_t cap) {
    if (!s || !out || cap == 0) return -1;
    return snprintf(out, cap,
        "{\"n\":%d,\"scale\":%.6f,\"rmse\":%.6f,\"rel_rmse\":%.6f}",
        s->n, (double)s->scale, (double)s->rmse, (double)s->rel_rmse);
}

/* ==================================================================
 * Self-test
 * ================================================================ */

#define _CHECK(cond, msg) do { \
    if (!(cond)) { \
        fprintf(stderr, "v130 self-test FAIL: %s (line %d)\n", (msg), __LINE__); \
        return 1; \
    } \
} while (0)

static float synthetic_lora_value(int i) {
    /* Deterministic pseudo-Gaussian-ish: sinusoid mix. */
    float s = sinf(0.31f * (float)i) * 0.7f + cosf(0.17f * (float)i) * 0.3f;
    return s * 0.05f;   /* typical LoRA Δ magnitude */
}

int cos_v130_self_test(void) {
    /* --- FP4 round-trip ----------------------------------------- */
    fprintf(stderr, "check-v130: FP4 LoRA-delta quantization round-trip\n");
    enum { N = 1024 };
    float  src[N];
    float  dst[N];
    uint8_t packed[(N + 1) / 2];
    for (int i = 0; i < N; ++i) src[i] = synthetic_lora_value(i);

    float scale = cos_v130_quantize_fp4(src, N, packed);
    cos_v130_dequantize_fp4(packed, N, scale, dst);

    double se = 0.0;
    for (int i = 0; i < N; ++i) {
        double d = (double)src[i] - (double)dst[i];
        se += d * d;
    }
    float rmse    = (float)sqrt(se / N);
    float relrmse = rmse / scale;
    _CHECK(relrmse < 0.10f, "FP4 rel-rmse under 0.10·scale");
    _CHECK(sizeof(packed) * 8 == N * 4, "FP4 uses 4 bits / value");

    /* --- σ-profile pack round-trip ------------------------------ */
    fprintf(stderr, "check-v130: σ-profile 8-byte pack round-trip\n");
    float ch_in[COS_V130_SIGMA_DIM] = {
        0.00f, 0.25f, 0.33f, 0.50f, 0.66f, 0.75f, 0.90f, 1.00f };
    uint8_t codes[COS_V130_SIGMA_DIM];
    float ch_out[COS_V130_SIGMA_DIM];
    cos_v130_pack_sigma  (ch_in , codes);
    cos_v130_unpack_sigma(codes , ch_out);
    for (int i = 0; i < COS_V130_SIGMA_DIM; ++i) {
        float e = fabsf(ch_out[i] - ch_in[i]);
        _CHECK(e <= 1.0f/255.0f + 1e-6f, "σ-pack round-trip ≤ 1/255");
    }

    /* --- PQ encode/decode preserves cosine ---------------------- */
    fprintf(stderr, "check-v130: PQ train + encode/decode cosine\n");
    enum { NV = 200 };
    float *train = (float*)calloc(NV * COS_V130_EMBED_DIM, sizeof(float));
    _CHECK(train != NULL, "malloc train");
    /* Synthetic low-rank data: 4 prototypes + jitter → encourages
     * distinct clusters. */
    for (int v = 0; v < NV; ++v) {
        int proto = v % 4;
        for (int d = 0; d < COS_V130_EMBED_DIM; ++d) {
            float base = sinf(0.01f * (float)d + (float)proto);
            float jit  = 0.01f * sinf(1.13f * (float)v + 0.07f * (float)d);
            train[v * COS_V130_EMBED_DIM + d] = base + jit;
        }
    }
    cos_v130_pq_codebook_t *cb = (cos_v130_pq_codebook_t*)calloc(1, sizeof *cb);
    _CHECK(cb != NULL, "malloc cb");
    int rc = cos_v130_pq_train(cb, train, NV, 6, 0xDEADBEEFULL);
    _CHECK(rc == 0 && cb->trained, "pq train ok");

    uint8_t cc[COS_V130_PQ_SUBSPACES];
    float  *recon = (float*)malloc(sizeof(float) * COS_V130_EMBED_DIM);
    _CHECK(recon != NULL, "malloc recon");
    rc = cos_v130_pq_encode(cb, train, cc);
    _CHECK(rc == 0, "pq encode ok");
    rc = cos_v130_pq_decode(cb, cc, recon);
    _CHECK(rc == 0, "pq decode ok");

    double dot = 0.0, n1 = 0.0, n2 = 0.0;
    for (int d = 0; d < COS_V130_EMBED_DIM; ++d) {
        double a = train[d];
        double b = recon[d];
        dot += a * b; n1 += a * a; n2 += b * b;
    }
    double cos_sim = dot / (sqrt(n1) * sqrt(n2) + 1e-12);
    _CHECK(cos_sim > 0.90, "PQ recon cosine > 0.90 on synthetic");
    free(train); free(recon); free(cb);

    /* --- σ-aware context compression ---------------------------- */
    fprintf(stderr, "check-v130: σ-aware context compression\n");
    cos_v130_chunk_t chunks[3] = {0};
    /* chunk 0: confident (low σ) — should be truncated more. */
    strncpy(chunks[0].text,
        "The capital of France is Paris. It was founded around the third century BC "
        "by a Celtic tribe known as the Parisii.",
        sizeof chunks[0].text - 1);
    chunks[0].sigma_product = 0.10f;
    /* chunk 1: uncertain — keeps more context. */
    strncpy(chunks[1].text,
        "The user mentioned a recent quantum experiment with unclear provenance and "
        "multiple conflicting references that may require further verification.",
        sizeof chunks[1].text - 1);
    chunks[1].sigma_product = 0.85f;
    strncpy(chunks[2].text,
        "Another confident sentence about basic arithmetic.",
        sizeof chunks[2].text - 1);
    chunks[2].sigma_product = 0.10f;

    char   cbuf[512];
    int    nw  = cos_v130_compress_context(chunks, 3, 180, cbuf, sizeof cbuf);
    _CHECK(nw > 0 && nw < (int)sizeof cbuf, "compress ok");
    size_t total_in = strlen(chunks[0].text) + strlen(chunks[1].text)
                    + strlen(chunks[2].text);
    _CHECK((size_t)nw < total_in, "compressed smaller than input");
    /* Uncertain chunk's first words must survive (larger allocation). */
    _CHECK(strstr(cbuf, "quantum") != NULL,
           "high-σ chunk content preserved");

    fprintf(stderr, "check-v130: OK (FP4 + σ-pack + PQ + context codec)\n");
    return 0;
}
