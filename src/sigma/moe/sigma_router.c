/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/* ULTRA-1 — σ-MoE adaptive expert routing (router uncertainty → top-k). */

#include "sigma_router.h"

#include <math.h>
#include <string.h>

void cos_ultra_sigma_router_softmax(const float *logits, int n, float *probs) {
    if (!logits || !probs || n < 1) return;
    float m = logits[0];
    for (int i = 1; i < n; ++i) {
        float v = logits[i];
        if (v == v && v > m) m = v;
    }
    double sum = 0.0;
    for (int i = 0; i < n; ++i) {
        float v = logits[i];
        if (v != v) {
            probs[i] = 0.0f;
            continue;
        }
        double e = exp((double)(v - m));
        probs[i] = (float)e;
        sum += e;
    }
    if (sum <= 0.0) {
        probs[0] = 1.0f;
        for (int i = 1; i < n; ++i) probs[i] = 0.0f;
        return;
    }
    float inv = (float)(1.0 / sum);
    for (int i = 0; i < n; ++i) probs[i] *= inv;
}

float cos_ultra_sigma_routing_uncertainty(const float *probs, int n) {
    if (!probs || n < 1) return 1.0f;
    float mx = probs[0];
    for (int i = 1; i < n; ++i) {
        float v = probs[i];
        if (v == v && v > mx) mx = v;
    }
    if (mx != mx) return 1.0f;
    float u = 1.0f - mx;
    if (u < 0.0f) u = 0.0f;
    if (u > 1.0f) u = 1.0f;
    return u;
}

int cos_ultra_sigma_router_adaptive_k(float sigma_routing, float tau,
                                      int n_experts) {
    if (n_experts < 1) return 1;
    if (!(tau > 0.0f)) tau = 0.25f;
    if (sigma_routing != sigma_routing) return 1;
    int k;
    if (sigma_routing < tau * 0.25f) k = 1;
    else if (sigma_routing < tau * 0.50f) k = 2;
    else if (sigma_routing < tau) k = 4;
    else k = 8;
    if (k > n_experts) k = n_experts;
    return k;
}

int cos_ultra_sigma_router_self_test(void) {
    float logits[] = { 1.0f, 2.0f, 0.5f, -1.0f };
    float p[4];
    cos_ultra_sigma_router_softmax(logits, 4, p);
    float s = 0.0f;
    for (int i = 0; i < 4; ++i) s += p[i];
    if (fabsf(s - 1.0f) > 1e-4f) return 1;
    float u = cos_ultra_sigma_routing_uncertainty(p, 4);
    if (u <= 0.0f || u >= 1.0f) return 2;
    int k = cos_ultra_sigma_router_adaptive_k(u, 0.30f, 8);
    if (k < 1 || k > 8) return 3;
    /* Peaked distribution → low uncertainty → k=1 */
    float z[] = { 10.0f, -10.0f, -10.0f };
    float q[3];
    cos_ultra_sigma_router_softmax(z, 3, q);
    float u2 = cos_ultra_sigma_routing_uncertainty(q, 3);
    int k2 = cos_ultra_sigma_router_adaptive_k(u2, 0.30f, 8);
    if (k2 != 1) return 4;
    return 0;
}
