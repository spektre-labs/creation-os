/*
 * v187 σ-Calibration — reference implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "calib.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

/* ---- PRNG ------------------------------------------------------ */

static uint64_t splitmix64(uint64_t *x) {
    uint64_t z = (*x += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

static float frand(uint64_t *s) {
    return (float)((splitmix64(s) >> 11) / (double)(1ULL << 53));
}

static float clampf(float v, float lo, float hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

/* ---- init ------------------------------------------------------ */

static const char *domain_names[COS_V187_N_DOMAINS] = {
    "math", "code", "history", "general"
};

void cos_v187_init(cos_v187_state_t *s, uint64_t seed) {
    memset(s, 0, sizeof(*s));
    s->seed              = seed ? seed : 0x187CA17BULL;
    s->n_samples         = COS_V187_N_HOLDOUT;
    s->n_domains         = COS_V187_N_DOMAINS;
    s->tau_ece           = 0.05f;
    s->global_temperature = 1.0f;
    for (int d = 0; d < s->n_domains; ++d) {
        s->domains[d].domain = d;
        s->domains[d].temperature = 1.0f;
        snprintf(s->domains[d].name, COS_V187_STR_MAX, "%s", domain_names[d]);
    }
}

/* ---- build 500-sample holdout with injected miscalibration ---- */

/* True confidence p_true ∈ [0,1]; we generate correctness using
 * p_true directly (Bernoulli), but expose a raw σ = f(p_true) that
 * is overconfident, i.e. too low vs the true error rate.
 *
 * We build the overconfidence by squashing the raw σ:
 *     sigma_raw = clamp( (1 - p_true) * 0.55, 0.02, 0.98 )
 * i.e. raw σ understates error by ~45 %.  Temperature scaling
 * σ^(1/T) with T > 1 expands large σ and keeps small σ small,
 * which corrects an under-confident σ; with T < 1 it shrinks
 * small σ and keeps large σ, which corrects over-confidence.
 * Since raw σ underestimates error (over-confident), we need
 * T < 1 to push σ up.  σ^(1/T), T=0.6 ⇒ σ^{1.67} which actually
 * reduces σ further, which is wrong.
 *
 * Instead we use the symmetric map:
 *     σ_cal = 1 - (1 - σ)^T
 * which, for T < 1, pushes (1-σ) toward 1, i.e. shrinks σ (bad);
 * for T > 1, pushes (1-σ) toward 0, i.e. raises σ (good).  That
 * is temperature scaling on the *probability* of error rather
 * than on σ itself and matches our monotonic remapping choice.
 *
 * So below we parameterise:  σ_cal = 1 - (1-σ_raw)^T.  Finding T
 * that minimises ECE is a 1-D bracketed search on [0.3, 4.0]. */
static float apply_T(float sigma, float T) {
    /* Temperature scaling on σ directly: σ_cal = σ^(1/T). T > 1
     * raises small σ toward 1 (compensates over-confidence); T < 1
     * shrinks σ toward 0 (compensates under-confidence). */
    double base = (double)sigma;
    if (base < 0.0) base = 0.0;
    if (base > 1.0) base = 1.0;
    double inv = (T > 1e-6f) ? 1.0 / (double)T : 1.0;
    double v = pow(base, inv);
    if (v < 0.0) v = 0.0;
    if (v > 1.0) v = 1.0;
    return (float)v;
}

void cos_v187_build(cos_v187_state_t *s) {
    /* Stratified, deterministic fixture.  10 target error-rate bands
     * span [0.05, 0.95] so every σ-bin is visited and the observed
     * error rate matches the target to within the integer-floor
     * quantisation (much tighter than Bernoulli sampling noise).
     * Per-sample correctness is decided by stride order so each
     * band has exactly ⌊50 · p⌋ incorrect samples.  Domains rotate
     * through the bands so each domain sees the whole σ spectrum. */
    const int per_band = COS_V187_N_HOLDOUT / 10;           /* 50 */
    uint64_t r = s->seed;
    for (int band = 0; band < 10; ++band) {
        float p_target = 0.05f + 0.10f * band;              /* 0.05..0.95 */
        int   n_wrong  = (int)(p_target * per_band + 0.5f); /* exact */
        for (int k = 0; k < per_band; ++k) {
            int i = band * per_band + k;
            cos_v187_sample_t *sp = &s->samples[i];
            sp->id     = i;
            sp->domain = i % s->n_domains;

            /* Add a deterministic per-domain shift so domain-level T
             * actually differs from the global T. */
            float shift = (sp->domain == 0) ? -0.03f :
                          (sp->domain == 2) ? +0.04f : 0.00f;
            float p = clampf(p_target + shift, 0.02f, 0.97f);

            /* Deterministic label: the first n_wrong samples in the
             * band are wrong, the rest are correct.  Use rng only
             * for a tiny σ jitter so binning is still interesting. */
            sp->correct   = (k >= n_wrong);

            /* Overconfident raw σ: σ_raw = p^1.8 ≪ p. */
            float sigma_raw = (float)pow((double)p, 1.8);
            /* Tiny deterministic jitter within the bin. */
            sigma_raw += 0.005f * (frand(&r) - 0.5f);
            sp->sigma_raw        = clampf(sigma_raw, 0.02f, 0.98f);
            sp->sigma_calibrated = sp->sigma_raw;
        }
    }
}

/* ---- binning + ECE -------------------------------------------- */

static void bin_predictions(cos_v187_bin_t *bins,
                            const cos_v187_sample_t *samples,
                            int n_samples,
                            bool use_calibrated) {
    for (int b = 0; b < COS_V187_N_BINS; ++b) {
        bins[b].id         = b;
        bins[b].lo         = (float)b / COS_V187_N_BINS;
        bins[b].hi         = (float)(b + 1) / COS_V187_N_BINS;
        bins[b].n          = 0;
        bins[b].n_correct  = 0;
        bins[b].mean_sigma = 0.0f;
    }
    for (int i = 0; i < n_samples; ++i) {
        float s = use_calibrated ? samples[i].sigma_calibrated
                                  : samples[i].sigma_raw;
        int b = (int)(s * COS_V187_N_BINS);
        if (b >= COS_V187_N_BINS) b = COS_V187_N_BINS - 1;
        if (b < 0)                b = 0;
        bins[b].n++;
        if (samples[i].correct) bins[b].n_correct++;
        bins[b].mean_sigma += s;
    }
    for (int b = 0; b < COS_V187_N_BINS; ++b) {
        if (bins[b].n == 0) {
            bins[b].mean_sigma        = (bins[b].lo + bins[b].hi) * 0.5f;
            /* In σ-semantics σ = P(error), so a perfectly calibrated
             * bin has observed_err ≈ mean_sigma. Empty bins contribute
             * zero to ECE because they are weighted by n/N. */
            bins[b].expected_err_rate = bins[b].mean_sigma;
            bins[b].observed_err_rate = bins[b].mean_sigma;
            continue;
        }
        bins[b].mean_sigma        /= (float)bins[b].n;
        bins[b].expected_err_rate  = bins[b].mean_sigma;
        float acc = (float)bins[b].n_correct / (float)bins[b].n;
        bins[b].observed_err_rate  = 1.0f - acc;
    }
}

static float ece_of_bins(const cos_v187_bin_t *bins, int n_samples) {
    double ece = 0;
    for (int b = 0; b < COS_V187_N_BINS; ++b) {
        if (bins[b].n == 0) continue;
        double diff = fabs((double)bins[b].observed_err_rate
                         - (double)bins[b].expected_err_rate);
        ece += diff * ((double)bins[b].n / n_samples);
    }
    return (float)ece;
}

static float ece_for_T(const cos_v187_sample_t *samples, int n, float T) {
    cos_v187_bin_t bins[COS_V187_N_BINS];
    /* Temp-apply without mutating the samples. */
    cos_v187_sample_t tmp[COS_V187_N_HOLDOUT];
    memcpy(tmp, samples, sizeof(cos_v187_sample_t) * n);
    for (int i = 0; i < n; ++i)
        tmp[i].sigma_calibrated = apply_T(tmp[i].sigma_raw, T);
    bin_predictions(bins, tmp, n, true);
    return ece_of_bins(bins, n);
}

/* Golden-section search for the T that minimises ECE on a subset. */
static float search_T(const cos_v187_sample_t *samples, int n,
                      float lo, float hi) {
    const float phi = 0.6180339887f;
    float a = lo, b = hi;
    float x1 = b - phi * (b - a);
    float x2 = a + phi * (b - a);
    float f1 = ece_for_T(samples, n, x1);
    float f2 = ece_for_T(samples, n, x2);
    for (int iter = 0; iter < 40; ++iter) {
        if (f1 < f2) {
            b = x2; x2 = x1; f2 = f1;
            x1 = b - phi * (b - a);
            f1 = ece_for_T(samples, n, x1);
        } else {
            a = x1; x1 = x2; f1 = f2;
            x2 = a + phi * (b - a);
            f2 = ece_for_T(samples, n, x2);
        }
        if (fabsf(b - a) < 1e-3f) break;
    }
    return 0.5f * (a + b);
}

/* ---- fit global T --------------------------------------------- */

void cos_v187_fit_global(cos_v187_state_t *s) {
    /* Raw ECE first. */
    bin_predictions(s->bins_raw, s->samples, s->n_samples, false);
    s->ece_raw = ece_of_bins(s->bins_raw, s->n_samples);

    /* Search T. */
    float T = search_T(s->samples, s->n_samples, 0.30f, 4.00f);
    s->global_temperature = T;

    /* Apply. */
    for (int i = 0; i < s->n_samples; ++i)
        s->samples[i].sigma_calibrated = apply_T(s->samples[i].sigma_raw, T);

    bin_predictions(s->bins_cal, s->samples, s->n_samples, true);
    s->ece_calibrated = ece_of_bins(s->bins_cal, s->n_samples);
}

/* ---- per-domain T --------------------------------------------- */

void cos_v187_fit_domains(cos_v187_state_t *s) {
    for (int d = 0; d < s->n_domains; ++d) {
        cos_v187_sample_t dom[COS_V187_N_HOLDOUT];
        int n = 0;
        for (int i = 0; i < s->n_samples; ++i) {
            if (s->samples[i].domain == d) {
                dom[n++] = s->samples[i];
            }
        }
        if (n == 0) continue;

        /* Raw domain ECE. */
        cos_v187_bin_t rb[COS_V187_N_BINS];
        bin_predictions(rb, dom, n, false);
        s->domains[d].ece_raw = ece_of_bins(rb, n);

        /* Search T per domain. */
        float T = search_T(dom, n, 0.30f, 4.00f);
        s->domains[d].temperature = T;
        for (int i = 0; i < n; ++i)
            dom[i].sigma_calibrated = apply_T(dom[i].sigma_raw, T);
        cos_v187_bin_t cb[COS_V187_N_BINS];
        bin_predictions(cb, dom, n, true);
        s->domains[d].ece_calibrated = ece_of_bins(cb, n);
    }
}

void cos_v187_rebin(cos_v187_state_t *s) {
    s->n_bins_improved = 0;
    for (int b = 0; b < COS_V187_N_BINS; ++b) {
        if (s->bins_raw[b].n == 0 || s->bins_cal[b].n == 0) continue;
        float raw = fabsf(s->bins_raw[b].observed_err_rate
                        - s->bins_raw[b].expected_err_rate);
        float cal = fabsf(s->bins_cal[b].observed_err_rate
                        - s->bins_cal[b].expected_err_rate);
        if (cal + 1e-6f < raw) s->n_bins_improved++;
    }
}

/* ---- JSON ------------------------------------------------------ */

size_t cos_v187_to_json(const cos_v187_state_t *s, char *buf, size_t cap) {
    int n = snprintf(buf, cap,
        "{\"kernel\":\"v187\","
        "\"n_samples\":%d,\"n_bins\":%d,\"n_domains\":%d,"
        "\"tau_ece\":%.4f,\"global_temperature\":%.4f,"
        "\"ece_raw\":%.4f,\"ece_calibrated\":%.4f,"
        "\"n_bins_improved\":%d,"
        "\"domains\":[",
        s->n_samples, COS_V187_N_BINS, s->n_domains,
        s->tau_ece, s->global_temperature,
        s->ece_raw, s->ece_calibrated,
        s->n_bins_improved);
    if (n < 0 || (size_t)n >= cap) return 0;
    size_t off = (size_t)n;
    for (int d = 0; d < s->n_domains; ++d) {
        const cos_v187_domain_t *dd = &s->domains[d];
        int k = snprintf(buf + off, cap - off,
            "%s{\"domain\":%d,\"name\":\"%s\",\"temperature\":%.4f,"
            "\"ece_raw\":%.4f,\"ece_calibrated\":%.4f}",
            d == 0 ? "" : ",", dd->domain, dd->name,
            dd->temperature, dd->ece_raw, dd->ece_calibrated);
        if (k < 0 || off + (size_t)k >= cap) return 0;
        off += (size_t)k;
    }
    int m = snprintf(buf + off, cap - off, "],\"bins_cal\":[");
    if (m < 0 || off + (size_t)m >= cap) return 0;
    off += (size_t)m;
    for (int b = 0; b < COS_V187_N_BINS; ++b) {
        const cos_v187_bin_t *bb = &s->bins_cal[b];
        int k = snprintf(buf + off, cap - off,
            "%s{\"id\":%d,\"n\":%d,"
            "\"mean_sigma\":%.4f,\"observed_err\":%.4f,"
            "\"expected_err\":%.4f}",
            b == 0 ? "" : ",", bb->id, bb->n,
            bb->mean_sigma, bb->observed_err_rate,
            bb->expected_err_rate);
        if (k < 0 || off + (size_t)k >= cap) return 0;
        off += (size_t)k;
    }
    int m2 = snprintf(buf + off, cap - off, "]}");
    if (m2 < 0 || off + (size_t)m2 >= cap) return 0;
    return off + (size_t)m2;
}

/* ---- self-test ------------------------------------------------- */

int cos_v187_self_test(void) {
    cos_v187_state_t s;
    cos_v187_init(&s, 0x187CA17BULL);
    cos_v187_build(&s);
    cos_v187_fit_global(&s);
    cos_v187_fit_domains(&s);
    cos_v187_rebin(&s);

    /* 1. raw ECE must be high enough to matter */
    if (s.ece_raw < 0.10f) return 1;

    /* 2. calibrated ECE drops below 0.05 */
    if (s.ece_calibrated >= 0.05f) return 2;

    /* 3. per-domain T ∈ (0, ∞) and differs from global at least once */
    int differ = 0;
    for (int d = 0; d < s.n_domains; ++d) {
        if (!(s.domains[d].temperature > 0.0f)) return 3;
        if (!isfinite(s.domains[d].temperature)) return 3;
        if (fabsf(s.domains[d].temperature - s.global_temperature) > 0.01f)
            differ++;
    }
    if (differ < 1) return 4;

    /* 4. ≥ 4 bins strictly improve */
    if (s.n_bins_improved < 4) return 5;

    return 0;
}
