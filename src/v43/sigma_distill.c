/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "sigma_distill.h"

#include <math.h>
#include <stddef.h>

float v43_sigma_weight(const sigma_decomposed_t *teacher_sigma)
{
    if (!teacher_sigma) {
        return 0.0f;
    }
    if (teacher_sigma->epistemic > 0.7f) {
        return 0.0f;
    }
    if (teacher_sigma->epistemic < 0.2f && teacher_sigma->aleatoric < 0.3f) {
        return 1.0f;
    }
    if (teacher_sigma->epistemic < 0.3f && teacher_sigma->aleatoric > 0.5f) {
        return 0.5f;
    }
    return -0.3f;
}

static void v43_softmax(const float *logits, int n, float temperature, float *out)
{
    float t = temperature;
    if (t < 1e-6f) {
        t = 1e-6f;
    }
    float m = logits[0];
    for (int i = 1; i < n; i++) {
        if (logits[i] > m) {
            m = logits[i];
        }
    }
    float sum = 0.0f;
    for (int i = 0; i < n; i++) {
        float x = (logits[i] - m) / t;
        out[i] = expf(x);
        sum += out[i];
    }
    if (sum <= 0.0f) {
        float u = 1.0f / (float)n;
        for (int i = 0; i < n; i++) {
            out[i] = u;
        }
        return;
    }
    float inv = 1.0f / sum;
    for (int i = 0; i < n; i++) {
        out[i] *= inv;
    }
}

float v43_kl_forward_pt_qs(const float *teacher_logits, const float *student_logits, int n, float temperature)
{
    if (n < 1 || !teacher_logits || !student_logits) {
        return 0.0f;
    }
    enum { V43_KL_MAX_V = 4096 };
    if (n > V43_KL_MAX_V) {
        n = V43_KL_MAX_V;
    }
    float p[4096];
    float q[4096];
    v43_softmax(teacher_logits, n, temperature, p);
    v43_softmax(student_logits, n, temperature, q);
    float kl = 0.0f;
    const float eps = 1e-12f;
    for (int i = 0; i < n; i++) {
        float pi = p[i] < eps ? eps : p[i];
        float qi = q[i] < eps ? eps : q[i];
        kl += pi * (logf(pi) - logf(qi));
    }
    return kl;
}

float v43_kl_reverse_qt_ps(const float *teacher_logits, const float *student_logits, int n, float temperature)
{
    if (n < 1 || !teacher_logits || !student_logits) {
        return 0.0f;
    }
    enum { V43_KL_MAX_V = 4096 };
    if (n > V43_KL_MAX_V) {
        n = V43_KL_MAX_V;
    }
    float p[4096];
    float q[4096];
    v43_softmax(teacher_logits, n, temperature, p);
    v43_softmax(student_logits, n, temperature, q);
    float kl = 0.0f;
    const float eps = 1e-12f;
    for (int i = 0; i < n; i++) {
        float pi = p[i] < eps ? eps : p[i];
        float qi = q[i] < eps ? eps : q[i];
        kl += qi * (logf(qi) - logf(pi));
    }
    return kl;
}

float v43_sigma_distillation_loss(const float *student_logits, const float *teacher_logits,
                                  const sigma_decomposed_t *teacher_sigma, int vocab_size, float temperature)
{
    if (!student_logits || !teacher_logits || !teacher_sigma || vocab_size < 1) {
        return 0.0f;
    }
    float w = v43_sigma_weight(teacher_sigma);
    if (fabsf(w) < 1e-8f) {
        return 0.0f;
    }
    if (w < 0.0f) {
        float rkl = v43_kl_reverse_qt_ps(teacher_logits, student_logits, vocab_size, temperature);
        return w * rkl;
    }
    float kl = v43_kl_forward_pt_qs(teacher_logits, student_logits, vocab_size, temperature);
    return w * kl;
}
