/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "fast_sigma.h"

#include <math.h>
#include <string.h>

float v46_top1_minus_top2_margin(const float *logits, int n)
{
    if (!logits || n <= 0) {
        return 0.0f;
    }
    float top1 = logits[0];
    float top2 = -INFINITY;
    for (int i = 1; i < n; i++) {
        float v = logits[i];
        if (v > top1) {
            top2 = top1;
            top1 = v;
        } else if (v > top2) {
            top2 = v;
        }
    }
    if (!isfinite(top2)) {
        top2 = top1;
    }
    return top1 - top2;
}

float v46_softmax_entropy_from_logits(const float *logits, int n)
{
    if (!logits || n <= 0) {
        return 0.0f;
    }
    float m = logits[0];
    for (int i = 1; i < n; i++) {
        if (logits[i] > m) {
            m = logits[i];
        }
    }
    double sum = 0.0;
    for (int i = 0; i < n; i++) {
        sum += exp((double)(logits[i] - m));
    }
    if (!(sum > 0.0)) {
        return 0.0f;
    }
    double acc = 0.0;
    for (int i = 0; i < n; i++) {
        double p = exp((double)(logits[i] - m)) / sum;
        if (p > 1e-12) {
            acc -= p * log(p);
        }
    }
    return (float)acc;
}

void v46_fast_sigma_from_logits(const float *logits, int vocab_size, sigma_decomposed_t *out)
{
    if (!out) {
        return;
    }
    memset(out, 0, sizeof *out);
    if (!logits || vocab_size <= 0) {
        return;
    }
    sigma_decompose_dirichlet_evidence(logits, vocab_size, out);
    float H = v46_softmax_entropy_from_logits(logits, vocab_size);
    float margin = v46_top1_minus_top2_margin(logits, vocab_size);
    float mnorm = tanhf(margin * 0.20f);
    if (mnorm < 0.0f) {
        mnorm = 0.0f;
    }
    if (mnorm > 1.0f) {
        mnorm = 1.0f;
    }
    out->total = H * 0.4f + (1.0f - mnorm) * 0.3f + out->epistemic * 0.3f;
}

void v46_latency_profile_reset(v46_latency_profile_t *p)
{
    if (!p) {
        return;
    }
    memset(p, 0, sizeof *p);
}

void v46_latency_profile_add(v46_latency_profile_t *p, float token_latency_ms, float sigma_overhead_ms)
{
    if (!p) {
        return;
    }
    if (!(token_latency_ms > 0.0f) || !isfinite(token_latency_ms)) {
        return;
    }
    p->token_latency_ms += token_latency_ms;
    p->sigma_overhead_ms += sigma_overhead_ms;
    float pct = 100.0f * (sigma_overhead_ms / token_latency_ms);
    if (pct > p->sigma_overhead_pct) {
        p->sigma_overhead_pct = pct;
    }
}
