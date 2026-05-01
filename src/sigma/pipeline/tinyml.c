/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/* σ-TinyML primitive. */

#include "tinyml.h"

#include <math.h>
#include <string.h>

static float clamp01(float x) {
    if (x != x)   return 1.0f;
    if (x < 0.0f) return 0.0f;
    if (x > 1.0f) return 1.0f;
    return x;
}

static uint16_t quantise(float sigma) {
    float s = clamp01(sigma) * 65535.0f + 0.5f;
    if (s < 0.0f)     s = 0.0f;
    if (s > 65535.0f) s = 65535.0f;
    return (uint16_t)s;
}

void cos_sigma_tinyml_init(cos_sigma_tinyml_state_t *s) {
    if (!s) return;
    memset(s, 0, sizeof *s);
}

void cos_sigma_tinyml_reset(cos_sigma_tinyml_state_t *s) {
    cos_sigma_tinyml_init(s);
}

float cos_sigma_tinyml_observe(cos_sigma_tinyml_state_t *s, float value) {
    if (!s) return 1.0f;

    /* NaN / ±inf input: flag as anomalous, don't poison state. */
    if (!(value == value)) {                /* NaN */
        s->last_sigma_q = quantise(1.0f);
        return 1.0f;
    }
    if (value == (1.0f / 0.0f) || value == -(1.0f / 0.0f)) {
        s->last_sigma_q = quantise(1.0f);
        return 1.0f;
    }

    /* Welford online update. */
    uint16_t n = (uint16_t)(s->count + 1);
    if (n == 0) n = UINT16_MAX;             /* saturate, don't wrap */

    /* σ is computed BEFORE absorbing the sample (so a spike is
     * detected against the mean built from history, not the mean
     * that already contains the spike). */
    float sigma = 0.0f;
    if (s->count >= 2) {
        float var = s->m2 / (float)(s->count - 1);
        float std = sqrtf(var);
        float z   = fabsf(value - s->mean)
                  / (std + COS_SIGMA_TINYML_EPS);
        sigma = clamp01(z / COS_SIGMA_TINYML_Z_ALERT);
    }

    /* Now absorb (Welford). */
    float delta  = value - s->mean;
    s->mean     += delta / (float)n;
    float delta2 = value - s->mean;
    s->m2       += delta * delta2;
    s->count     = n;
    s->last_sigma_q = quantise(sigma);
    return sigma;
}

bool cos_sigma_tinyml_is_anomalous(const cos_sigma_tinyml_state_t *s,
                                   float threshold) {
    if (!s) return true;
    float sig = (float)s->last_sigma_q / 65535.0f;
    return sig > threshold;
}

float cos_sigma_tinyml_last_sigma(const cos_sigma_tinyml_state_t *s) {
    if (!s) return 1.0f;
    return (float)s->last_sigma_q / 65535.0f;
}

/* --- self-test --------------------------------------------------------- */

static int check_size(void) {
    if (sizeof(cos_sigma_tinyml_state_t) != 12) return 10;
    return 0;
}

static int check_bootstrap(void) {
    cos_sigma_tinyml_state_t s;
    cos_sigma_tinyml_init(&s);

    /* First sample: σ = 0 (no history). */
    float sig0 = cos_sigma_tinyml_observe(&s, 20.0f);
    if (sig0 != 0.0f)        return 20;
    if (s.count != 1)        return 21;

    /* Second sample: σ still 0 (variance undefined with n-1 = 1). */
    float sig1 = cos_sigma_tinyml_observe(&s, 20.5f);
    if (sig1 != 0.0f)        return 22;
    if (s.count != 2)        return 23;
    return 0;
}

static int check_stable_stream_low_sigma(void) {
    /* A stable temperature stream ~20°C ± 0.1°C.  With Z_ALERT = 3
     * a 1σ deviation maps to σ ≈ 0.33 and a 2σ deviation to σ ≈ 0.67.
     * So we allow up to 2σ wiggle (sig ≤ 0.70) post-bootstrap. */
    cos_sigma_tinyml_state_t s;
    cos_sigma_tinyml_init(&s);
    float xs[] = {20.0f, 20.1f, 19.9f, 20.0f, 20.1f, 20.0f,
                  19.9f, 20.0f, 20.1f, 20.0f};
    for (int i = 0; i < (int)(sizeof xs / sizeof xs[0]); ++i) {
        float sig = cos_sigma_tinyml_observe(&s, xs[i]);
        if (i >= 4 && sig > 0.70f) return 30 + i;
    }
    return 0;
}

static int check_spike_high_sigma(void) {
    /* Build a stable baseline then inject a big spike. */
    cos_sigma_tinyml_state_t s;
    cos_sigma_tinyml_init(&s);
    for (int i = 0; i < 8; ++i) cos_sigma_tinyml_observe(&s, 20.0f);
    /* Even 8 identical samples: variance is ~0, std ≈ eps.
     * A large deviation should trip σ → 1.0. */
    float sig = cos_sigma_tinyml_observe(&s, 40.0f);
    if (sig < 0.99f) return 40;
    if (!cos_sigma_tinyml_is_anomalous(&s, 0.9f)) return 41;
    return 0;
}

static int check_nan_preserves_state(void) {
    cos_sigma_tinyml_state_t s;
    cos_sigma_tinyml_init(&s);
    for (int i = 0; i < 5; ++i) cos_sigma_tinyml_observe(&s, 10.0f);
    cos_sigma_tinyml_state_t before = s;

    float nan_v = 0.0f / 0.0f;
    float sig = cos_sigma_tinyml_observe(&s, nan_v);
    if (sig != 1.0f) return 50;

    /* mean, m2, count unchanged; last_sigma_q updated. */
    if (s.mean  != before.mean)  return 51;
    if (s.m2    != before.m2)    return 52;
    if (s.count != before.count) return 53;
    if (s.last_sigma_q == before.last_sigma_q) return 54;
    if (cos_sigma_tinyml_last_sigma(&s) < 0.999f) return 55;
    return 0;
}

static int check_reset(void) {
    cos_sigma_tinyml_state_t s;
    cos_sigma_tinyml_init(&s);
    for (int i = 0; i < 5; ++i) cos_sigma_tinyml_observe(&s, 7.0f);
    cos_sigma_tinyml_reset(&s);
    if (s.count != 0 || s.m2 != 0.0f || s.mean != 0.0f
                     || s.last_sigma_q != 0) return 60;
    return 0;
}

static int check_lcg_stress(void) {
    /* 1000 Gaussian-like draws via box-muller from an LCG;
     * σ should rarely saturate (>99 out of 1000 shouldn't be > 0.99). */
    cos_sigma_tinyml_state_t s;
    cos_sigma_tinyml_init(&s);
    uint32_t state = 0xC0FFEE01u;
    int saturated = 0;
    for (int i = 0; i < 1000; ++i) {
        state = state * 1664525u + 1013904223u;
        float u1 = ((float)((state >> 8) & 0xFFFF) + 1.0f) / 65537.0f;
        state = state * 1664525u + 1013904223u;
        float u2 = ((float)((state >> 8) & 0xFFFF) + 1.0f) / 65537.0f;
        /* Box-Muller → N(0, 1). */
        float z = sqrtf(-2.0f * logf(u1)) * cosf(6.2831853f * u2);
        float v = 20.0f + z;                 /* mean 20, std 1 */
        float sig = cos_sigma_tinyml_observe(&s, v);
        if (sig > 0.99f) ++saturated;
    }
    /* At Z_ALERT = 3 the Gaussian tail beyond 3σ is ~0.27% → ≤ 10
     * saturations expected in 1000 samples; allow 30 for LCG jitter. */
    if (saturated > 30) return 70;
    return 0;
}

int cos_sigma_tinyml_self_test(void) {
    int rc;
    if ((rc = check_size()))                     return rc;
    if ((rc = check_bootstrap()))                return rc;
    if ((rc = check_stable_stream_low_sigma()))  return rc;
    if ((rc = check_spike_high_sigma()))         return rc;
    if ((rc = check_nan_preserves_state()))      return rc;
    if ((rc = check_reset()))                    return rc;
    if ((rc = check_lcg_stress()))               return rc;
    return 0;
}
