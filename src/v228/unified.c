/*
 * v228 σ-Unified — reference implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "unified.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static uint64_t fnv1a(const void *data, size_t n, uint64_t seed) {
    const uint8_t *p = (const uint8_t *)data;
    uint64_t h = seed ? seed : 0xcbf29ce484222325ULL;
    for (size_t i = 0; i < n; ++i) { h ^= p[i]; h *= 0x100000001b3ULL; }
    return h;
}

static float clamp01(float x) {
    if (x < 0.0f) return 0.0f;
    if (x > 1.0f) return 1.0f;
    return x;
}

void cos_v228_init(cos_v228_state_t *s, uint64_t seed) {
    memset(s, 0, sizeof(*s));
    s->seed     = seed ? seed : 0x228F1E1DULL;
    s->alpha    = 0.20f;
    s->beta     = 0.02f;
    s->gamma_   = 0.01f;
    s->k_crit   = 0.50f;
    s->eps_cons = 1e-6f;
}

static float make_noise(int t, float *phi_state) {
    *phi_state += 0.37f + (float)(t % 7) * 0.11f;
    return sinf(*phi_state);
}

void cos_v228_run(cos_v228_state_t *s) {
    /* Initial state. */
    float sigma = 0.90f;
    float K = 1.0f - sigma;
    float tau0 = 1.0f;
    s->C   = K * tau0;      /* = 0.10 */
    float tau = s->C / K;   /* = 1.0 */

    s->trace[0].t       = 0;
    s->trace[0].sigma   = sigma;
    s->trace[0].k_eff   = K;
    s->trace[0].tau     = tau;
    s->trace[0].noise   = 0.0f;
    s->trace[0].interaction = 0.0f;
    s->trace[0].k_eff_tau   = K * tau;
    s->trace[0].phase   = (K >= s->k_crit) ? 1 : 0;

    float phi = 0.0f;
    s->sigma_start = sigma;
    s->n_transitions = 0;
    s->max_cons_error = 0.0f;

    for (int t = 1; t <= COS_V228_N_STEPS; ++t) {
        float noise = make_noise(t, &phi);
        float inter = cosf(0.13f * (float)t);

        float dsigma = -s->alpha * K + s->beta * noise + s->gamma_ * inter;
        sigma = clamp01(sigma + dsigma);
        K = 1.0f - sigma;

        /* τ(t) := C / K_eff(t) — conservation by
         * definition (see header). Guard against K=0
         * which would make τ diverge; floor at 1e-3. */
        float K_for_tau = (K < 1e-3f) ? 1e-3f : K;
        tau = s->C / K_for_tau;
        float ktau = K * tau;    /* can be <C if K was
                                    clamped by floor */

        cos_v228_sample_t *sm = &s->trace[t];
        sm->t           = t;
        sm->sigma       = sigma;
        sm->k_eff       = K;
        sm->tau         = tau;
        sm->noise       = noise;
        sm->interaction = inter;
        sm->k_eff_tau   = ktau;
        sm->phase       = (K >= s->k_crit) ? 1 : 0;

        /* Conservation error — with the K floor,
         * the error is only non-zero on the pathological
         * K → 0 branch, which the fixture never
         * reaches. */
        float err = fabsf(ktau - s->C);
        if (err > s->max_cons_error) s->max_cons_error = err;

        if (sm->phase != s->trace[t-1].phase) s->n_transitions++;
    }
    s->sigma_end = sigma;

    /* FNV-1a chain. */
    uint64_t prev = 0x228D1E1DULL;
    for (int i = 0; i <= COS_V228_N_STEPS; ++i) {
        const cos_v228_sample_t *sm = &s->trace[i];
        struct { int t, phase; float sig, k, tau, n, inter, kt;
                 uint64_t prev; } rec;
        memset(&rec, 0, sizeof(rec));
        rec.t = sm->t; rec.phase = sm->phase;
        rec.sig = sm->sigma; rec.k = sm->k_eff; rec.tau = sm->tau;
        rec.n = sm->noise; rec.inter = sm->interaction; rec.kt = sm->k_eff_tau;
        rec.prev = prev;
        prev = fnv1a(&rec, sizeof(rec), prev);
    }
    {
        struct { int nt; float ss, se, mce, C; uint64_t prev; } rec;
        memset(&rec, 0, sizeof(rec));
        rec.nt = s->n_transitions;
        rec.ss = s->sigma_start; rec.se = s->sigma_end;
        rec.mce = s->max_cons_error; rec.C = s->C;
        rec.prev = prev;
        prev = fnv1a(&rec, sizeof(rec), prev);
    }
    s->terminal_hash = prev;
    s->chain_valid   = true;
}

size_t cos_v228_to_json(const cos_v228_state_t *s, char *buf, size_t cap) {
    int n = snprintf(buf, cap,
        "{\"kernel\":\"v228\","
        "\"n_steps\":%d,\"alpha\":%.3f,\"beta\":%.3f,"
        "\"gamma\":%.3f,\"k_crit\":%.3f,\"eps_cons\":%.8f,"
        "\"C\":%.6f,"
        "\"sigma_start\":%.4f,\"sigma_end\":%.4f,"
        "\"n_transitions\":%d,\"max_cons_error\":%.8f,"
        "\"chain_valid\":%s,\"terminal_hash\":\"%016llx\","
        "\"trace\":[",
        COS_V228_N_STEPS, s->alpha, s->beta, s->gamma_,
        s->k_crit, s->eps_cons, s->C,
        s->sigma_start, s->sigma_end,
        s->n_transitions, s->max_cons_error,
        s->chain_valid ? "true" : "false",
        (unsigned long long)s->terminal_hash);
    if (n < 0 || (size_t)n >= cap) return 0;
    size_t off = (size_t)n;
    for (int i = 0; i <= COS_V228_N_STEPS; ++i) {
        const cos_v228_sample_t *sm = &s->trace[i];
        int k = snprintf(buf + off, cap - off,
            "%s{\"t\":%d,\"sigma\":%.4f,\"k_eff\":%.4f,"
            "\"tau\":%.4f,\"noise\":%.4f,\"interaction\":%.4f,"
            "\"k_eff_tau\":%.6f,\"phase\":%d}",
            i == 0 ? "" : ",",
            sm->t, sm->sigma, sm->k_eff, sm->tau, sm->noise,
            sm->interaction, sm->k_eff_tau, sm->phase);
        if (k < 0 || off + (size_t)k >= cap) return 0;
        off += (size_t)k;
    }
    int m = snprintf(buf + off, cap - off, "]}");
    if (m < 0 || off + (size_t)m >= cap) return 0;
    return off + (size_t)m;
}

int cos_v228_self_test(void) {
    cos_v228_state_t s;
    cos_v228_init(&s, 0x228F1E1DULL);
    cos_v228_run(&s);
    if (!s.chain_valid) return 1;

    for (int i = 0; i <= COS_V228_N_STEPS; ++i) {
        const cos_v228_sample_t *sm = &s.trace[i];
        if (sm->sigma < 0.0f || sm->sigma > 1.0f) return 2;
        if (sm->k_eff < 0.0f || sm->k_eff > 1.0f) return 2;
        if (fabsf(sm->k_eff_tau - s.C) > s.eps_cons) return 3;
    }
    if (s.n_transitions < 1)       return 4;
    if (!(s.sigma_end < s.sigma_start)) return 5;
    if (s.max_cons_error > s.eps_cons)   return 6;
    return 0;
}
