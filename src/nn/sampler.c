/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "cos_sampler.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#if defined(__aarch64__) && defined(__ARM_NEON)
#include <arm_neon.h>
#endif

enum { COS_SAMPLER_ALIGN = 64 };

static void *cos_alloc_aligned64(size_t nbytes)
{
    if (nbytes == 0u)
        return NULL;
    size_t align = (size_t)COS_SAMPLER_ALIGN;
    size_t n = (nbytes + align - 1u) / align * align;
    return aligned_alloc(align, n);
}

#if defined(__aarch64__) && defined(__ARM_NEON)
static float vec_max_f32_neon(const float *x, int n)
{
    if (n <= 0)
        return 0.f;
    if (n < 4) {
        float m = x[0];
        for (int i = 1; i < n; i++)
            if (x[i] > m)
                m = x[i];
        return m;
    }
    float32x4_t m4 = vld1q_f32(x);
    int i = 4;
    for (; i + 16 <= n; i += 16) {
        __builtin_prefetch(x + i + 64, 0, 3);
        float32x4_t a0 = vld1q_f32(x + i);
        float32x4_t a1 = vld1q_f32(x + i + 4);
        float32x4_t a2 = vld1q_f32(x + i + 8);
        float32x4_t a3 = vld1q_f32(x + i + 12);
        float32x4_t mx = vmaxq_f32(vmaxq_f32(a0, a1), vmaxq_f32(a2, a3));
        m4 = vmaxq_f32(m4, mx);
    }
    for (; i + 4 <= n; i += 4)
        m4 = vmaxq_f32(m4, vld1q_f32(x + i));
    float m = vmaxvq_f32(m4);
    for (; i < n; i++)
        if (x[i] > m)
            m = x[i];
    return m;
}
#endif

static float softmax_row(const float *logits, int n, float temp, float *probs)
{
#if defined(__aarch64__) && defined(__ARM_NEON)
    float m = vec_max_f32_neon(logits, n);
#else
    float m = logits[0];
    for (int i = 1; i < n; i++)
        if (logits[i] > m)
            m = logits[i];
#endif
    double sum = 0.0;
    for (int i = 0; i < n; i++) {
        double z = (double)((logits[i] - m) / (temp > 1e-6f ? temp : 1e-6f));
        double e = exp(z);
        probs[i] = (float)e;
        sum += e;
    }
    if (sum <= 0.0) {
        float u = 1.f / (float)n;
        for (int i = 0; i < n; i++)
            probs[i] = u;
        return (float)log((double)n);
    }
    for (int i = 0; i < n; i++)
        probs[i] = (float)((double)probs[i] / sum);
    double h = 0.0;
    for (int i = 0; i < n; i++) {
        double p = (double)probs[i];
        if (p > 1e-12)
            h -= p * log(p);
    }
    return (float)h;
}

float cos_logits_entropy(const float *logits, int n)
{
    if (!logits || n <= 0)
        return 0.f;
    float *tmp = (float *)cos_alloc_aligned64(sizeof(float) * (size_t)n);
    if (!tmp)
        return 0.f;
    float h = softmax_row(logits, n, 1.f, tmp);
    free(tmp);
    return h;
}

static unsigned int urand(unsigned int *st)
{
    if (st)
        return (unsigned int)rand_r(st);
    return (unsigned int)rand();
}

int cos_sample_logits(const float *logits, int vocab, float temperature, int topk, float topp, unsigned int *rnd_state,
                      uint32_t *out_id)
{
    if (!logits || vocab <= 0 || !out_id)
        return -1;
    float *p = (float *)cos_alloc_aligned64(sizeof(float) * (size_t)vocab * 2u);
    if (!p)
        return -1;
    float *work = p + vocab;
    memcpy(work, logits, sizeof(float) * (size_t)vocab);

    /* top-k mask by zeroing non-top */
    if (topk > 0 && topk < vocab) {
        for (int pass = 0; pass < vocab - topk; pass++) {
            int mi = 0;
            for (int i = 1; i < vocab; i++)
                if (work[i] < work[mi])
                    mi = i;
            work[mi] = -INFINITY;
        }
    }

    (void)softmax_row(work, vocab, temperature > 0.f ? temperature : 1.f, p);

    /* nucleus top-p (minimal, correct cumulative mass on sorted probabilities). */
    if (topp > 0.f && topp < 1.f) {
        int *idx = (int *)malloc(sizeof(int) * (size_t)vocab);
        if (!idx) {
            free(p);
            return -1;
        }
        for (int i = 0; i < vocab; i++)
            idx[i] = i;
        for (int a = 0; a < vocab; a++) {
            for (int b = a + 1; b < vocab; b++) {
                if (p[idx[b]] > p[idx[a]]) {
                    int t = idx[a];
                    idx[a] = idx[b];
                    idx[b] = t;
                }
            }
        }
        unsigned char *keep = (unsigned char *)calloc((size_t)vocab, 1);
        if (!keep) {
            free(idx);
            free(p);
            return -1;
        }
        double cum = 0.0;
        for (int i = 0; i < vocab; i++) {
            int t = idx[i];
            keep[t] = 1u;
            cum += (double)p[t];
            if (cum >= (double)topp)
                break;
        }
        for (int i = 0; i < vocab; i++)
            if (!keep[i])
                p[i] = 0.f;
        free(keep);
        free(idx);
        double s = 0.0;
        for (int i = 0; i < vocab; i++)
            s += (double)p[i];
        if (s > 0.0) {
            for (int i = 0; i < vocab; i++)
                p[i] = (float)((double)p[i] / s);
        }
    }

    /* sample */
    unsigned int r = urand(rnd_state);
    float u = (float)((double)(r & 0xffffff) / (double)0x1000000);
    float acc = 0.f;
    for (int i = 0; i < vocab; i++) {
        acc += p[i];
        if (u <= acc) {
            *out_id = (uint32_t)i;
            free(p);
            return 0;
        }
    }
    *out_id = (uint32_t)(vocab - 1);
    free(p);
    return 0;
}
