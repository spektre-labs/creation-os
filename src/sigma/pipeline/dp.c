/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/* σ-Differential-Privacy implementation. See dp.h for the
 * mechanism and contract notes.  This module is deterministic: the
 * RNG is a counter-based LCG seeded by the caller, so a replayed
 * (seed, gradient) pair yields a bit-identical perturbed vector
 * across hosts / compilers (assuming IEEE-754 double precision in
 * the log() call, which every tier-1 platform honours).
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "dp.h"

#include <math.h>
#include <string.h>

/* ------------------------------------------------------------------ *
 *  Numerics helpers
 * ------------------------------------------------------------------ */

static inline int is_finite_f(float v) {
    return (v == v) && (v != (float)(1.0/0.0)) && (v != -(float)(1.0/0.0));
}

static inline float clip01(float v) {
    if (v < 0.0f) return 0.0f;
    if (v > 1.0f) return 1.0f;
    return v;
}

static float l2_norm(const float *v, int n) {
    double acc = 0.0;
    for (int i = 0; i < n; i++) acc += (double)v[i] * (double)v[i];
    return (float)sqrt(acc);
}

/* ------------------------------------------------------------------ *
 *  Counter-based LCG → uniform(-0.5, 0.5) → Laplace(scale)
 *
 *  The generator is intentionally simple (not cryptographic): same
 *  (seed, call-index) pair produces the same stream on every host.
 *  SplitMix64 constants; the top 53 bits feed a double.
 * ------------------------------------------------------------------ */

static uint64_t splitmix64(uint64_t *s) {
    uint64_t z = (*s += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

/* Uniform in (-0.5, 0.5), avoiding exact -0.5 and 0.5 (keeps
 * log-argument strictly in (0, 1)). */
static double uniform_centered(uint64_t *s) {
    uint64_t z = splitmix64(s);
    /* 53-bit mantissa → [0, 1) */
    double u = (double)(z >> 11) * (1.0 / (double)(1ULL << 53));
    /* Shift to (-0.5, 0.5) and clamp to avoid the boundary. */
    u -= 0.5;
    if (u <= -0.499999999999) u = -0.499999999999;
    if (u >=  0.499999999999) u =  0.499999999999;
    return u;
}

/* Inverse CDF of Laplace(0, scale):
 *     F^{-1}(u) = -scale · sign(u) · ln(1 - 2|u|),   u ∈ (-0.5, 0.5).
 */
static float laplace_sample(double scale, uint64_t *s) {
    double u = uniform_centered(s);
    double a = (u < 0.0) ? -u : u;
    double lg = log(1.0 - 2.0 * a);
    double x = (u < 0.0) ? ( scale * lg) : (-scale * lg);
    return (float)x;
}

/* ------------------------------------------------------------------ *
 *  Public API
 * ------------------------------------------------------------------ */

int cos_sigma_dp_init(cos_sigma_dp_state_t *st,
                      float epsilon,
                      float delta,
                      float clip_norm,
                      float epsilon_total,
                      uint64_t seed)
{
    if (!st) return COS_DP_ERR_ARG;
    if (!(epsilon > 0.0f) || !is_finite_f(epsilon))        return COS_DP_ERR_ARG;
    if (!(delta >= 0.0f) || !(delta < 1.0f))               return COS_DP_ERR_ARG;
    if (!(clip_norm > 0.0f) || !is_finite_f(clip_norm))    return COS_DP_ERR_ARG;
    if (!(epsilon_total > 0.0f) ||
        !is_finite_f(epsilon_total))                       return COS_DP_ERR_ARG;
    if (epsilon > epsilon_total)                           return COS_DP_ERR_ARG;

    st->epsilon       = epsilon;
    st->delta         = delta;
    st->clip_norm     = clip_norm;
    st->epsilon_spent = 0.0f;
    st->epsilon_total = epsilon_total;
    st->rng_state     = seed ? seed : 0x1ULL;
    st->rounds        = 0;
    return COS_DP_OK;
}

int cos_sigma_dp_add_noise(cos_sigma_dp_state_t *st,
                           float *grad,
                           int n_params,
                           cos_sigma_dp_report_t *report)
{
    if (report) memset(report, 0, sizeof(*report));
    if (!st || !grad || n_params <= 0) {
        if (report) report->status = COS_DP_ERR_ARG;
        return COS_DP_ERR_ARG;
    }
    for (int i = 0; i < n_params; i++) {
        if (!is_finite_f(grad[i])) {
            if (report) report->status = COS_DP_ERR_NAN;
            return COS_DP_ERR_NAN;
        }
    }

    /* Budget check first — a BUDGET_EXHAUSTED round leaves `grad`
     * untouched. */
    if (st->epsilon_spent + st->epsilon > st->epsilon_total + 1e-6f) {
        if (report) {
            report->status            = COS_DP_ERR_BUDGET_EXHAUSTED;
            report->epsilon_remaining = st->epsilon_total - st->epsilon_spent;
            report->budget_exhausted  = 1;
        }
        return COS_DP_ERR_BUDGET_EXHAUSTED;
    }

    float pre_norm = l2_norm(grad, n_params);
    float C        = st->clip_norm;
    float scale    = (pre_norm > C) ? (C / pre_norm) : 1.0f;

    /* Store a local copy of g_clipped so we can measure noise L2
     * post-hoc. */
    enum { STACK_CAP = 4096 };
    static float heap_scratch[STACK_CAP];  /* single-threaded usage */
    float *g_clip = (n_params <= STACK_CAP) ? heap_scratch : NULL;
    if (!g_clip) {
        if (report) report->status = COS_DP_ERR_ARG;
        return COS_DP_ERR_ARG;
    }

    for (int i = 0; i < n_params; i++) g_clip[i] = grad[i] * scale;
    float post_clip = l2_norm(g_clip, n_params);

    /* Laplace perturbation. */
    double lap_scale = (double)C / (double)st->epsilon;
    for (int i = 0; i < n_params; i++) {
        float n = laplace_sample(lap_scale, &st->rng_state);
        grad[i] = g_clip[i] + n;
    }

    float post_noise = l2_norm(grad, n_params);

    /* noise L2 = ‖g_p − g_c‖ */
    double acc = 0.0;
    for (int i = 0; i < n_params; i++) {
        double d = (double)grad[i] - (double)g_clip[i];
        acc += d * d;
    }
    float noise_l2 = (float)sqrt(acc);
    float sigma_dp = clip01(noise_l2 / (post_clip + 1e-7f));

    /* Commit budget. */
    st->epsilon_spent += st->epsilon;
    st->rounds        += 1;

    if (report) {
        report->status            = COS_DP_OK;
        report->clip_scale        = scale;
        report->noise_scale       = (float)lap_scale;
        report->pre_clip_norm     = pre_norm;
        report->post_clip_norm    = post_clip;
        report->post_noise_norm   = post_noise;
        report->noise_l2          = noise_l2;
        report->sigma_dp          = sigma_dp;
        report->epsilon_remaining = st->epsilon_total - st->epsilon_spent;
        report->budget_exhausted  =
            (report->epsilon_remaining + 1e-6f < st->epsilon) ? 1 : 0;
    }
    return COS_DP_OK;
}

float cos_sigma_dp_compute_sigma(const float *g_clipped,
                                 const float *g_perturbed,
                                 int n_params)
{
    if (!g_clipped || !g_perturbed || n_params <= 0) return 0.0f;
    double acc = 0.0;
    double cn  = 0.0;
    for (int i = 0; i < n_params; i++) {
        double d = (double)g_perturbed[i] - (double)g_clipped[i];
        acc += d * d;
        cn  += (double)g_clipped[i] * (double)g_clipped[i];
    }
    return clip01((float)(sqrt(acc) / (sqrt(cn) + 1e-7)));
}

/* ------------------------------------------------------------------ *
 *  Self-test — determinism + budget exhaustion + σ_dp monotonicity
 * ------------------------------------------------------------------ */

int cos_sigma_dp_self_test(void) {
    /* 1. Deterministic replay: same seed + same gradient → bit-
     *    identical output. */
    float g1[8] = { 0.5f, -0.3f, 0.2f, 0.1f, -0.4f, 0.0f, 0.6f, -0.1f };
    float g2[8]; memcpy(g2, g1, sizeof(g1));

    cos_sigma_dp_state_t a, b;
    cos_sigma_dp_init(&a, /*ε=*/1.0f, /*δ=*/1e-6f,
                      /*clip=*/1.0f, /*ε_total=*/5.0f, /*seed=*/0xC05DULL);
    cos_sigma_dp_init(&b, 1.0f, 1e-6f, 1.0f, 5.0f, 0xC05DULL);

    cos_sigma_dp_report_t ra, rb;
    if (cos_sigma_dp_add_noise(&a, g1, 8, &ra) != COS_DP_OK) return -1;
    if (cos_sigma_dp_add_noise(&b, g2, 8, &rb) != COS_DP_OK) return -2;

    for (int i = 0; i < 8; i++)
        if (g1[i] != g2[i]) return -3;

    /* 2. Budget exhaustion: after 5 rounds of ε=1 against ε_total=5
     *    the 6th add_noise must refuse. */
    float g3[4] = { 0.1f, 0.1f, 0.1f, 0.1f };
    cos_sigma_dp_state_t c;
    cos_sigma_dp_init(&c, 1.0f, 1e-6f, 1.0f, 5.0f, 0xDEADBEEFULL);
    cos_sigma_dp_report_t rc;
    for (int i = 0; i < 5; i++) {
        float tmp[4]; memcpy(tmp, g3, sizeof(g3));
        if (cos_sigma_dp_add_noise(&c, tmp, 4, &rc) != COS_DP_OK) return -4;
    }
    float tmp[4]; memcpy(tmp, g3, sizeof(g3));
    int rv = cos_sigma_dp_add_noise(&c, tmp, 4, &rc);
    if (rv != COS_DP_ERR_BUDGET_EXHAUSTED)  return -5;
    if (!rc.budget_exhausted)               return -6;
    /* gradient must be untouched */
    for (int i = 0; i < 4; i++) if (tmp[i] != g3[i]) return -7;

    /* 3. σ_dp monotonicity: larger ε (smaller noise) ⇒ smaller σ_dp. */
    float go[16]; for (int i = 0; i < 16; i++) go[i] = 0.2f;
    float gp[16]; for (int i = 0; i < 16; i++) gp[i] = 0.2f;
    cos_sigma_dp_state_t lo, hi;
    cos_sigma_dp_init(&lo, /*ε=*/0.1f, 1e-6f, 1.0f, 1.0f, 0x11ULL);
    cos_sigma_dp_init(&hi, /*ε=*/1.0f, 1e-6f, 1.0f, 1.0f, 0x11ULL);
    cos_sigma_dp_report_t rl, rh;
    cos_sigma_dp_add_noise(&lo, go, 16, &rl);
    cos_sigma_dp_add_noise(&hi, gp, 16, &rh);
    if (!(rl.sigma_dp >= rh.sigma_dp)) return -8;
    if (!(rl.noise_scale > rh.noise_scale)) return -9;

    /* 4. Arg-error triage. */
    if (cos_sigma_dp_init(NULL, 1.0f, 0.0f, 1.0f, 1.0f, 1) != COS_DP_ERR_ARG) return -10;
    cos_sigma_dp_state_t d;
    if (cos_sigma_dp_init(&d, 0.0f, 0.0f, 1.0f, 1.0f, 1) != COS_DP_ERR_ARG) return -11;
    if (cos_sigma_dp_init(&d, 1.0f, 1.5f, 1.0f, 1.0f, 1) != COS_DP_ERR_ARG) return -12;
    if (cos_sigma_dp_init(&d, 1.0f, 0.0f, 0.0f, 1.0f, 1) != COS_DP_ERR_ARG) return -13;
    if (cos_sigma_dp_init(&d, 2.0f, 0.0f, 1.0f, 1.0f, 1) != COS_DP_ERR_ARG) return -14;

    return 0;
}
