/*
 * v193 σ-Coherence — reference implementation.
 *
 *   Trajectory (16 ticks):
 *     t  0..5  : steady state, K_eff above K_crit
 *     t  6..8  : controlled σ spike drives K_eff below K_crit
 *                (v150 swarm consensus miscalibration scenario)
 *     t  9     : v159 heal triggers, σ drops back
 *     t 10..15 : recovery + calibration improvement (v187 ECE
 *                drops → F rises → K_eff rises monotonically)
 *
 *   All values are closed-form deterministic functions of t.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "coherence.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static float clampf(float v, float lo, float hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static float geom3(float a, float b, float c) {
    if (a < 1e-6f) a = 1e-6f;
    if (b < 1e-6f) b = 1e-6f;
    if (c < 1e-6f) c = 1e-6f;
    return (float)cbrtf(a * b * c);
}

void cos_v193_init(cos_v193_state_t *s, uint64_t seed) {
    memset(s, 0, sizeof(*s));
    s->seed    = seed ? seed : 0x193C0A1EULL;
    s->n_ticks = COS_V193_N_TICKS;
    s->K_crit  = 0.25f;                 /* operational floor */
    s->first_alert_tick = -1;
    s->recovery_tick    = -1;
    s->recovery_lag     = -1;
}

/* ---- trajectory builder ------------------------------------- */

void cos_v193_build(cos_v193_state_t *s) {
    for (int t = 0; t < s->n_ticks; ++t) {
        cos_v193_tick_t *k = &s->ticks[t];
        k->t       = t;
        k->n_pairs = COS_V193_N_PAIRS;

        float rho, acc, ece, align, sigma, Hw, Hp;

        if (t <= 5) {
            /* steady state */
            rho   = 0.92f;
            Hw    = 2.10f;
            Hp    = 2.80f;
            acc   = 0.85f;
            ece   = 0.05f;
            align = 0.90f;
            sigma = 0.15f;
        } else if (t <= 8) {
            /* σ spike in the swarm consensus */
            rho   = 0.75f - 0.05f * (t - 6);
            Hw    = 1.80f;
            Hp    = 2.80f;
            acc   = 0.70f;
            ece   = 0.12f + 0.03f * (t - 6);
            align = 0.80f;
            sigma = 0.55f + 0.10f * (t - 6); /* climbs through 0.55..0.75 */
        } else if (t == 9) {
            /* v159 heal triggers mid-tick */
            rho   = 0.82f;
            Hw    = 2.00f;
            Hp    = 2.80f;
            acc   = 0.75f;
            ece   = 0.10f;
            align = 0.82f;
            sigma = 0.35f;
        } else {
            /* recovery + v187 ECE improvement → F rises */
            int r = t - 10;                 /* 0..5 */
            rho   = 0.85f + 0.01f * r;
            Hw    = 2.10f + 0.02f * r;
            Hp    = 2.80f;
            acc   = 0.82f + 0.01f * r;
            /* ECE monotone decreases. */
            ece   = 0.08f - 0.012f * r;
            if (ece < 0.01f) ece = 0.01f;
            align = 0.88f + 0.01f * r;
            sigma = 0.20f - 0.02f * r;
            if (sigma < 0.05f) sigma = 0.05f;
        }

        rho   = clampf(rho,   0.0f, 1.0f);
        acc   = clampf(acc,   0.0f, 1.0f);
        ece   = clampf(ece,   0.0f, 1.0f);
        align = clampf(align, 0.0f, 1.0f);
        sigma = clampf(sigma, 0.0f, 1.0f);

        /* Map rho onto discrete consistency count so JSON is integer. */
        k->rho                 = rho;
        k->n_consistent_pairs  = (int)floorf(rho * k->n_pairs + 0.5f);
        k->H_whole             = Hw;
        k->H_parts_sum         = Hp;
        k->accuracy            = acc;
        k->ece                 = ece;
        k->alignment           = align;
        k->sigma               = sigma;
    }
}

/* ---- run: compute K, K_eff, alert trajectory --------------- */

void cos_v193_run(cos_v193_state_t *s) {
    s->n_alerts           = 0;
    s->K_eff_min          = 1e9f;
    s->K_eff_max          = -1e9f;
    s->first_alert_tick   = -1;
    s->recovery_tick      = -1;
    s->recovery_lag       = -1;
    s->all_components_in_range = true;
    s->K_eff_identity_holds    = true;
    s->improvement_phase_monotone = true;

    for (int t = 0; t < s->n_ticks; ++t) {
        cos_v193_tick_t *k = &s->ticks[t];

        /* I_Φ = min(1, H_whole / H_parts_sum). */
        float iphi = (k->H_parts_sum > 1e-6f) ?
                     k->H_whole / k->H_parts_sum : 0.0f;
        if (iphi < 0.0f) iphi = 0.0f;
        if (iphi > 1.0f) iphi = 1.0f;
        k->i_phi = iphi;

        k->F     = geom3(k->accuracy, 1.0f - k->ece, k->alignment);

        k->K     = k->rho * k->i_phi * k->F;
        k->K_eff = (1.0f - k->sigma) * k->K;
        k->alert = (k->K_eff < s->K_crit);

        /* identity check. */
        float expected = (1.0f - k->sigma) * k->K;
        if (fabsf(expected - k->K_eff) > 1e-6f)
            s->K_eff_identity_holds = false;

        /* range checks. */
        float arr[7] = { k->rho, k->i_phi, k->F, k->sigma,
                          k->K, k->K_eff, k->accuracy };
        for (int i = 0; i < 7; ++i) {
            if (arr[i] < -1e-6f || arr[i] > 1.0f + 1e-6f)
                s->all_components_in_range = false;
        }

        if (k->K_eff < s->K_eff_min) s->K_eff_min = k->K_eff;
        if (k->K_eff > s->K_eff_max) s->K_eff_max = k->K_eff;

        if (k->alert) {
            s->n_alerts++;
            if (s->first_alert_tick < 0) s->first_alert_tick = t;
        } else if (s->first_alert_tick >= 0 && s->recovery_tick < 0) {
            s->recovery_tick = t;
            s->recovery_lag  = t - s->first_alert_tick;
        }
    }

    /* Monotone rise in improvement phase t10..t15. */
    for (int t = 11; t < s->n_ticks; ++t) {
        if (!(s->ticks[t].K_eff > s->ticks[t-1].K_eff - 1e-6f)) {
            s->improvement_phase_monotone = false;
            break;
        }
    }

    s->delta_K_eff_after_calibration =
        s->ticks[s->n_ticks - 1].K_eff - s->ticks[10].K_eff;
}

/* ---- JSON -------------------------------------------------- */

size_t cos_v193_to_json(const cos_v193_state_t *s, char *buf, size_t cap) {
    int n = snprintf(buf, cap,
        "{\"kernel\":\"v193\","
        "\"n_ticks\":%d,\"K_crit\":%.3f,"
        "\"n_alerts\":%d,\"first_alert_tick\":%d,"
        "\"recovery_tick\":%d,\"recovery_lag\":%d,"
        "\"K_eff_min\":%.4f,\"K_eff_max\":%.4f,"
        "\"delta_K_eff_after_calibration\":%.4f,"
        "\"all_components_in_range\":%s,"
        "\"K_eff_identity_holds\":%s,"
        "\"improvement_phase_monotone\":%s,"
        "\"ticks\":[",
        s->n_ticks, s->K_crit,
        s->n_alerts, s->first_alert_tick,
        s->recovery_tick, s->recovery_lag,
        s->K_eff_min, s->K_eff_max,
        s->delta_K_eff_after_calibration,
        s->all_components_in_range     ? "true" : "false",
        s->K_eff_identity_holds        ? "true" : "false",
        s->improvement_phase_monotone  ? "true" : "false");
    if (n < 0 || (size_t)n >= cap) return 0;
    size_t off = (size_t)n;
    for (int t = 0; t < s->n_ticks; ++t) {
        const cos_v193_tick_t *k = &s->ticks[t];
        int kk = snprintf(buf + off, cap - off,
            "%s{\"t\":%d,"
            "\"rho\":%.4f,\"i_phi\":%.4f,\"F\":%.4f,"
            "\"accuracy\":%.4f,\"ece\":%.4f,\"alignment\":%.4f,"
            "\"sigma\":%.4f,"
            "\"K\":%.4f,\"K_eff\":%.4f,"
            "\"alert\":%s,\"n_pairs\":%d,\"n_consistent_pairs\":%d}",
            t == 0 ? "" : ",", k->t,
            k->rho, k->i_phi, k->F,
            k->accuracy, k->ece, k->alignment,
            k->sigma, k->K, k->K_eff,
            k->alert ? "true" : "false",
            k->n_pairs, k->n_consistent_pairs);
        if (kk < 0 || off + (size_t)kk >= cap) return 0;
        off += (size_t)kk;
    }
    int m = snprintf(buf + off, cap - off, "]}");
    if (m < 0 || off + (size_t)m >= cap) return 0;
    return off + (size_t)m;
}

/* ---- self-test -------------------------------------------- */

int cos_v193_self_test(void) {
    cos_v193_state_t s;
    cos_v193_init(&s, 0x193C0A1EULL);
    cos_v193_build(&s);
    cos_v193_run(&s);

    if (s.n_ticks != COS_V193_N_TICKS)       return 1;
    if (!s.all_components_in_range)           return 2;
    if (!s.K_eff_identity_holds)              return 3;
    if (s.n_alerts < 1)                       return 4;
    if (s.first_alert_tick < 0)               return 5;
    if (s.recovery_tick < 0)                  return 6;
    if (s.recovery_lag > 3)                   return 7;    /* heal ≤ 3 ticks */
    if (!s.improvement_phase_monotone)        return 8;
    if (!(s.delta_K_eff_after_calibration > 0.0f)) return 9;
    return 0;
}
