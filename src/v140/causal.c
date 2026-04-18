/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (c) 2026 Spektre Labs / Lauri Rainio
 *
 * v140 σ-Causal — implementation.
 */
#include "causal.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* =============== 1. counterfactual propagation ================== */

int cos_v140_counterfactual(const cos_v139_model_t *m,
                            const float *state_now,
                            int index, float value,
                            cos_v140_counterfactual_t *out) {
    if (!m || !state_now || !out) return -1;
    int D = m->dim;
    if (index < 0 || index >= D) return -1;
    memset(out, 0, sizeof *out);
    out->dim = D;
    out->intervened_index = index;
    out->intervened_value = value;

    float s_nat[COS_V139_MAX_DIM];
    float s_do [COS_V139_MAX_DIM];
    float n_nat[COS_V139_MAX_DIM];
    float n_do [COS_V139_MAX_DIM];
    memcpy(s_nat, state_now, (size_t)D * sizeof(float));
    memcpy(s_do,  state_now, (size_t)D * sizeof(float));
    s_do[index] = value;

    cos_v139_predict(m, s_nat, n_nat);
    cos_v139_predict(m, s_do,  n_do);

    double dn = 0.0, nn = 0.0;
    for (int i = 0; i < D; ++i) {
        double d = (double)n_do[i] - (double)n_nat[i];
        dn += d * d;
        nn += (double)n_nat[i] * (double)n_nat[i];
    }
    out->delta_norm   = (float)sqrt(dn);
    out->natural_norm = (float)sqrt(nn);
    double denom = out->natural_norm > 1e-9 ? out->natural_norm : 1e-9;
    out->sigma_causal = (float)(sqrt(dn) / denom);
    return 0;
}

/* =============== 2. σ-channel ablation attribution ============== */

float cos_v140_weighted_geomean(const float *w, const float *s, int n) {
    if (!w || !s || n <= 0) return 1.0f;
    double log_acc = 0.0, wsum = 0.0;
    for (int i = 0; i < n; ++i) {
        if (w[i] <= 0.0f) continue;
        double v = s[i];
        if (v <= 0.0)   v = 1e-6;
        if (v >  1.0)   v = 1.0;
        log_acc += (double)w[i] * log(v);
        wsum    += (double)w[i];
    }
    if (wsum <= 0.0) return 1.0f;
    return (float)exp(log_acc / wsum);
}

/* Insertion-sort top-k by descending |delta|. */
static void insert_top(cos_v140_channel_attr_t *top, int *count,
                       int cap, cos_v140_channel_attr_t cand) {
    /* find insertion position */
    int pos = *count;
    for (int i = 0; i < *count; ++i) {
        if (fabsf(cand.delta) > fabsf(top[i].delta)) { pos = i; break; }
    }
    if (pos >= cap) return;
    int limit = (*count < cap) ? *count : cap - 1;
    for (int i = limit; i > pos; --i) top[i] = top[i - 1];
    top[pos] = cand;
    if (*count < cap) (*count)++;
}

int cos_v140_attribute(const float *w, const float *sigma, int n,
                       float threshold,
                       cos_v140_attribution_t *out) {
    if (!w || !sigma || n <= 0 || n > COS_V140_MAX_CHANNELS || !out)
        return -1;
    memset(out, 0, sizeof *out);
    out->n_channels = n;
    out->threshold  = threshold;
    memcpy(out->weights, w,     (size_t)n * sizeof(float));
    memcpy(out->sigma,   sigma, (size_t)n * sizeof(float));

    float baseline = cos_v140_weighted_geomean(w, sigma, n);
    out->sigma_agg     = baseline;
    out->verdict_emit  = (baseline < threshold) ? 1 : 0;

    float deltas[COS_V140_MAX_CHANNELS];
    double abs_sum = 0.0;
    for (int i = 0; i < n; ++i) {
        float saved = sigma[i];
        /* ablation = remove the signal, i.e. set to the "no-abstain"
         * neutral value 1.0.  We cast away const locally. */
        float tmp[COS_V140_MAX_CHANNELS];
        memcpy(tmp, sigma, (size_t)n * sizeof(float));
        tmp[i] = 1.0f;
        float abl = cos_v140_weighted_geomean(w, tmp, n);
        deltas[i] = baseline - abl;
        abs_sum  += fabs((double)deltas[i]);
        (void)saved;
    }

    int k = 0;
    for (int i = 0; i < n; ++i) {
        cos_v140_channel_attr_t c;
        c.index   = i;
        c.delta   = deltas[i];
        c.percent = 0.0f; /* fill after we know top-k */
        insert_top(out->top, &k, COS_V140_TOPK, c);
    }
    out->top_k = k;

    /* Percent of *total* |delta| across all channels so the numbers
     * are comparable even when other channels matter too. */
    if (abs_sum > 0.0) {
        for (int i = 0; i < k; ++i)
            out->top[i].percent = (float)(
                100.0 * fabs((double)out->top[i].delta) / abs_sum);
    }
    return 0;
}

int cos_v140_attribution_to_json(const cos_v140_attribution_t *a,
                                 char *buf, size_t cap) {
    if (!a || !buf || cap == 0) return -1;
    size_t off = 0;
    int rc = snprintf(buf + off, cap - off,
        "{\"sigma_agg\":%.6f,\"threshold\":%.6f,\"verdict\":\"%s\","
        "\"n_channels\":%d,\"top\":[",
        (double)a->sigma_agg, (double)a->threshold,
        a->verdict_emit ? "emit" : "abstain", a->n_channels);
    if (rc < 0 || (size_t)rc >= cap - off) return -1;
    off += (size_t)rc;
    for (int i = 0; i < a->top_k; ++i) {
        rc = snprintf(buf + off, cap - off,
            "%s{\"index\":%d,\"delta\":%.6f,\"percent\":%.2f}",
            i ? "," : "", a->top[i].index,
            (double)a->top[i].delta, (double)a->top[i].percent);
        if (rc < 0 || (size_t)rc >= cap - off) return -1;
        off += (size_t)rc;
    }
    rc = snprintf(buf + off, cap - off, "]}");
    if (rc < 0 || (size_t)rc >= cap - off) return -1;
    off += (size_t)rc;
    return (int)off;
}

/* ================================================================
 * Self-test
 * ============================================================= */

#define _CHECK(cond, msg) do { \
    if (!(cond)) { \
        fprintf(stderr, "v140 self-test FAIL: %s (line %d)\n", \
                (msg), __LINE__); return 1; \
    } \
} while (0)

int cos_v140_self_test(void) {
    const int D = 16;
    const int Tp1 = 201;

    /* --- A. Build a v139 world model from a rotation trajectory --- */
    fprintf(stderr, "check-v140: v139 world model (D=%d)\n", D);
    float *traj = (float *)malloc((size_t)Tp1 * D * sizeof(float));
    _CHECK(traj, "alloc");
    cos_v139_synthetic_trajectory(traj, D, Tp1, 1.00f, 11ULL);
    cos_v139_model_t m;
    _CHECK(cos_v139_fit(&m, D, 150, traj, traj + D) == 0, "fit");

    /* --- B. Counterfactual: intervene on a known-active dim -------
     * On an orthogonal rotation, overwriting a component of the
     * input must produce a non-zero delta in the prediction —
     * and the magnitude scales with the perturbation size.         */
    cos_v140_counterfactual_t cf1, cf2;
    const float *s_now = traj + 42 * D;
    fprintf(stderr, "check-v140: counterfactual propagation\n");
    _CHECK(cos_v140_counterfactual(&m, s_now, 3,
                                   s_now[3] + 1.0f, &cf1) == 0, "cf1");
    _CHECK(cos_v140_counterfactual(&m, s_now, 3,
                                   s_now[3] + 2.0f, &cf2) == 0, "cf2");
    fprintf(stderr,
        "  cf1 Δ=%.4f natural=%.4f σ_causal=%.4f\n"
        "  cf2 Δ=%.4f natural=%.4f σ_causal=%.4f\n",
        (double)cf1.delta_norm, (double)cf1.natural_norm,
        (double)cf1.sigma_causal,
        (double)cf2.delta_norm, (double)cf2.natural_norm,
        (double)cf2.sigma_causal);
    _CHECK(cf1.delta_norm > 0.01f, "cf Δ>0 for nonzero intervention");
    _CHECK(cf2.delta_norm > cf1.delta_norm * 1.5f,
           "larger intervention → larger Δ (≥ 1.5×)");

    /* --- C. Ablation attribution: extreme-σ channels dominate ----
     * v140.0 attribution is log-geomean ablation Δ: |Δ_i| is largest
     * for channels whose σ is *farthest from neutral 1.0*, because
     * those are the channels that most move σ_agg when removed.  A
     * channel with σ ≈ 1.0 (near-neutral, weak signal) has |Δ| ≈ 0.
     *
     * Test fixture: 5 channels, 2 strong-emit signals (σ=0.20), one
     * near-neutral (σ=0.95), one moderate (σ=0.30), one low (σ=0.25).
     * Expected top-3: the extreme-σ channels, and the near-neutral
     * channel must NOT be in the top-3.                            */
    fprintf(stderr, "check-v140: σ-channel ablation attribution\n");
    float w[5] = { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f };
    /* n_eff, tail_mass, entropy, agree, drift */
    float sigma[5] = { 0.20f, 0.95f, 0.30f, 0.25f, 0.20f };
    cos_v140_attribution_t attr;
    _CHECK(cos_v140_attribute(w, sigma, 5, 0.50f, &attr) == 0, "attribute");
    fprintf(stderr, "  σ_agg=%.4f verdict=%s\n",
            (double)attr.sigma_agg, attr.verdict_emit ? "emit" : "abstain");
    for (int i = 0; i < attr.top_k; ++i)
        fprintf(stderr, "  top[%d]: ch=%d Δ=%.4f pct=%.2f%%\n",
                i, attr.top[i].index,
                (double)attr.top[i].delta,
                (double)attr.top[i].percent);
    _CHECK(attr.top_k == 3, "top-3 returned");
    /* The near-neutral channel (ch 1, σ=0.95) has |log σ| ≈ 0.05 and
     * must not be in the top-3: geomean-attribution ranks by how much
     * a channel deviates from the "no-signal" point 1.0. */
    _CHECK(attr.top[0].index != 1 && attr.top[1].index != 1
           && attr.top[2].index != 1,
           "near-neutral (σ≈1) channel must NOT be in top-3");
    /* Dominant channel must be one of the strong-emit signals (σ=0.20). */
    _CHECK(attr.top[0].index == 0 || attr.top[0].index == 4,
           "dominant channel is one of the σ=0.20 extremes");
    _CHECK(attr.top[0].percent > 20.0f,
           "dominant channel > 20% of total |Δ|");
    /* Percent shares sum to ≤ 100 (the top-3 is at most all of it). */
    double psum = (double)attr.top[0].percent
                + (double)attr.top[1].percent
                + (double)attr.top[2].percent;
    _CHECK(psum > 50.0 && psum <= 100.01,
           "top-3 together account for ≥50% of |Δ|");

    /* --- D. JSON shape ------------------------------------------- */
    char js[512];
    int jw = cos_v140_attribution_to_json(&attr, js, sizeof js);
    fprintf(stderr, "  json=%s\n", js);
    _CHECK(jw > 0, "json emit");
    _CHECK(strstr(js, "\"top\":[") != NULL, "json has top array");
    _CHECK(strstr(js, "\"verdict\":") != NULL, "json has verdict");

    free(traj);
    fprintf(stderr, "check-v140: OK (counterfactual + attribution)\n");
    return 0;
}
