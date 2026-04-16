/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "cos_sampler.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

static float softmax_row(const float *logits, int n, float temp, float *probs)
{
    float m = logits[0];
    for (int i = 1; i < n; i++)
        if (logits[i] > m)
            m = logits[i];
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
    float *tmp = (float *)malloc(sizeof(float) * (size_t)n);
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
    float *p = (float *)malloc(sizeof(float) * (size_t)vocab * 2u);
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
