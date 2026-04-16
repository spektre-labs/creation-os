/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "attention_xnor.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#if defined(__GNUC__) || defined(__clang__)
#define COS_POPCOUNTLL(x) __builtin_popcountll((x))
#else
static int cos_popcountll(uint64_t x)
{
    int c = 0;
    while (x) {
        x &= x - 1ull;
        c++;
    }
    return c;
}
#define COS_POPCOUNTLL(x) cos_popcountll((x))
#endif

void quantize_to_bsc(const int8_t *x, int d, uint64_t *out_words, int n_words)
{
    if (!x || !out_words || d <= 0 || n_words <= 0)
        return;
    for (int w = 0; w < n_words; w++) {
        uint64_t packed = 0ull;
        for (int b = 0; b < 64; b++) {
            int idx = (w * 64 + b) * d / (64 * n_words);
            if (idx < 0)
                idx = 0;
            if (idx >= d)
                idx = d - 1;
            if (x[idx] > 0)
                packed |= (1ull << (uint64_t)b);
        }
        out_words[w] = packed;
    }
}

float bsc_similarity(const uint64_t *a, const uint64_t *b, int n_words)
{
    if (!a || !b || n_words <= 0)
        return 0.f;
    int matches = 0;
    int bits = n_words * 64;
    for (int i = 0; i < n_words; i++)
        matches += COS_POPCOUNTLL(~(a[i] ^ b[i]));
    return (float)matches / (float)bits;
}

void bsc_bind(const uint64_t *a, const uint64_t *b, uint64_t *out, int n_words)
{
    if (!a || !b || !out || n_words <= 0)
        return;
    for (int i = 0; i < n_words; i++)
        out[i] = ~(a[i] ^ b[i]);
}

void attention_xnor(const int8_t *q, const int8_t *k, const int8_t *v, int8_t *out, int n_tokens, int d, int n_heads,
                    int bsc_words)
{
    if (!q || !k || !v || !out || n_tokens <= 0 || d <= 0 || n_heads <= 0 || (d % n_heads) != 0 || bsc_words <= 0)
        return;
    const int d_head = d / n_heads;
    uint64_t *q_bsc = (uint64_t *)calloc((size_t)n_tokens * (size_t)n_heads * (size_t)bsc_words, sizeof(uint64_t));
    uint64_t *k_bsc = (uint64_t *)calloc((size_t)n_tokens * (size_t)n_heads * (size_t)bsc_words, sizeof(uint64_t));
    if (!q_bsc || !k_bsc) {
        free(q_bsc);
        free(k_bsc);
        return;
    }
    for (int t = 0; t < n_tokens; t++) {
        for (int h = 0; h < n_heads; h++) {
            const int8_t *qp = q + t * d + h * d_head;
            const int8_t *kp = k + t * d + h * d_head;
            uint64_t *qb = q_bsc + ((t * n_heads + h) * bsc_words);
            uint64_t *kb = k_bsc + ((t * n_heads + h) * bsc_words);
            quantize_to_bsc(qp, d_head, qb, bsc_words);
            quantize_to_bsc(kp, d_head, kb, bsc_words);
        }
    }
    float *sims = (float *)calloc((size_t)n_tokens * (size_t)n_tokens * (size_t)n_heads, sizeof(float));
    if (!sims) {
        free(q_bsc);
        free(k_bsc);
        return;
    }
    for (int t = 0; t < n_tokens; t++) {
        for (int s = 0; s <= t; s++) {
            for (int h = 0; h < n_heads; h++) {
                const uint64_t *qa = q_bsc + ((t * n_heads + h) * bsc_words);
                const uint64_t *kb = k_bsc + ((s * n_heads + h) * bsc_words);
                sims[(t * n_tokens + s) * n_heads + h] = bsc_similarity(qa, kb, bsc_words);
            }
        }
    }
    memset(out, 0, (size_t)n_tokens * (size_t)d * sizeof(int8_t));
    for (int t = 0; t < n_tokens; t++) {
        for (int h = 0; h < n_heads; h++) {
            float total = 0.f;
            for (int s = 0; s <= t; s++)
                total += sims[(t * n_tokens + s) * n_heads + h];
            if (total < 1e-6f)
                total = 1.0f;
            for (int i = 0; i < d_head; i++) {
                float acc = 0.f;
                for (int s = 0; s <= t; s++) {
                    float w = sims[(t * n_tokens + s) * n_heads + h] / total;
                    acc += w * (float)v[s * d + h * d_head + i];
                }
                int vq = (int)lrintf(acc);
                if (vq > 127)
                    vq = 127;
                if (vq < -128)
                    vq = -128;
                out[t * d + h * d_head + i] = (int8_t)vq;
            }
        }
    }
    free(q_bsc);
    free(k_bsc);
    free(sims);
}
