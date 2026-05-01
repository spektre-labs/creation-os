/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/* ULTRA-11 — Parcae-style recurrent depth with measured σ halting. */

#include "recurrent_depth.h"

#include <math.h>

static float vec_norm2(const float *h, int dim) {
    double s = 0.0;
    for (int i = 0; i < dim; ++i) {
        double v = (double)h[i];
        s += v * v;
    }
    return (float)sqrt(s);
}

/* Toy σ(h): monotone in vector norm → recurrent shrink lowers σ. */
static float measure_sigma(const float *h, int dim) {
    float n = vec_norm2(h, dim);
    float s = n / (n + 1.0f);
    if (s > 0.999f) s = 0.999f;
    if (s < 0.0f) s = 0.0f;
    return s;
}

static void encode_h0(float *h, int dim, float scale) {
    for (int i = 0; i < dim; ++i) h[i] = scale;
}

static void transform_step(int pattern, int t, float gain, float *h, int dim) {
    if (pattern == 0) {
        for (int i = 0; i < dim; ++i) h[i] *= gain;
        return;
    }
    /* Pattern 1: contract then spike state (σ rises → overthink halt). */
    float g = 0.94f - 0.005f * (float)t;
    if (g < 0.60f) g = 0.60f;
    for (int i = 0; i < dim; ++i) h[i] *= g;
    if (t >= 5) {
        for (int i = 0; i < dim; ++i) h[i] += (i == 0) ? 1.0f : 0.08f;
    }
}

int cos_ultra_recurrent_depth_run(const cos_ultra_recurrent_params_t *p,
                                  int *out_steps,
                                  cos_ultra_recurrent_stop_t *out_reason,
                                  float *out_sigma_final,
                                  float *out_rho_last) {
    if (!p || !out_steps || !out_reason || !out_sigma_final || !out_rho_last)
        return -1;
    int dim = p->dim;
    if (dim < 1 || dim > 64) return -2;
    int t_max = p->t_max;
    if (t_max < 1) return -3;
    float tau = p->tau_accept;
    if (!(tau > 0.0f) || tau > 1.0f) return -4;

    float h[64];
    encode_h0(h, dim, p->h0_scale);
    float sigma_prev = NAN;
    float prev_norm = vec_norm2(h, dim);
    *out_rho_last = 0.0f;

    for (int t = 1; t <= t_max; ++t) {
        transform_step(p->pattern, t, p->transform_gain, h, dim);
        float sig = measure_sigma(h, dim);
        float nrm = vec_norm2(h, dim);
        if (prev_norm > 1e-6f)
            *out_rho_last = nrm / prev_norm;
        else
            *out_rho_last = 0.0f;
        prev_norm = nrm;

        /* Spectral guard: ρ̂ → 1 with flat/worsening σ (toy Parcae-style). */
        if (t > 2 && prev_norm > 1e-5f && *out_rho_last >= 0.995f &&
            *out_rho_last <= 1.02f && sig >= sigma_prev - 1e-4f &&
            sigma_prev == sigma_prev) {
            *out_steps = t;
            *out_reason = COS_ULTRA_REC_STOP_SPECTRAL;
            *out_sigma_final = sig;
            return 0;
        }

        if (sig < tau) {
            *out_steps = t;
            *out_reason = COS_ULTRA_REC_STOP_SIGMA_ACCEPT;
            *out_sigma_final = sig;
            return 0;
        }
        if (t > 3 && sigma_prev == sigma_prev && sig > sigma_prev) {
            *out_steps = t;
            *out_reason = COS_ULTRA_REC_STOP_OVERTHINK;
            *out_sigma_final = sig;
            return 0;
        }
        sigma_prev = sig;
    }
    *out_steps = t_max;
    *out_reason = COS_ULTRA_REC_STOP_MAX_ITER;
    *out_sigma_final = measure_sigma(h, dim);
    return 0;
}

int cos_ultra_recurrent_depth_self_test(void) {
    cos_ultra_recurrent_params_t p0 = {
        .t_max = 64, .tau_accept = 0.35f, .dim = 4,
        .h0_scale = 2.0f, .transform_gain = 0.88f, .pattern = 0,
    };
    int steps;
    cos_ultra_recurrent_stop_t why;
    float sigf, rho;
    if (cos_ultra_recurrent_depth_run(&p0, &steps, &why, &sigf, &rho) != 0)
        return 1;
    if (why != COS_ULTRA_REC_STOP_SIGMA_ACCEPT) return 2;
    if (steps < 1 || steps > p0.t_max) return 3;
    if (sigf >= p0.tau_accept) return 4;

    cos_ultra_recurrent_params_t p1 = {
        .t_max = 32, .tau_accept = 0.01f, /* never hit accept */
        .dim = 4, .h0_scale = 1.2f, .transform_gain = 0.9f, .pattern = 1,
    };
    if (cos_ultra_recurrent_depth_run(&p1, &steps, &why, &sigf, &rho) != 0)
        return 5;
    if (why != COS_ULTRA_REC_STOP_OVERTHINK) return 6;
    if (steps <= 3) return 7;
    return 0;
}
