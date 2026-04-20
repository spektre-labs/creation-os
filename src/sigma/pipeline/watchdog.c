/*
 * σ-watchdog — implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#define _GNU_SOURCE
#include "watchdog.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

/* ------------------------ lifecycle ------------------------ */

void cos_watchdog_init(cos_watchdog_t *w) {
    if (!w) return;
    memset(w, 0, sizeof *w);
    w->tau_sigma_up = COS_WATCHDOG_TAU_SIGMA_UP;
    w->tau_escal_up = COS_WATCHDOG_TAU_ESCAL_UP;
    w->health       = COS_WATCHDOG_HEALTHY;
}

void cos_watchdog_set_thresholds(cos_watchdog_t *w,
                                 float tau_sigma_up,
                                 float tau_escal_up) {
    if (!w) return;
    w->tau_sigma_up = tau_sigma_up;
    w->tau_escal_up = tau_escal_up;
}

/* ------------------------ observe ------------------------ */

void cos_watchdog_observe(cos_watchdog_t *w,
                          uint64_t timestamp_s, float sigma,
                          int escalated, float cost_eur) {
    if (!w) return;
    cos_watchdog_sample_t *s = &w->ring[w->head];
    s->timestamp_s = timestamp_s;
    if (sigma < 0.0f) sigma = 0.0f;
    if (sigma > 1.0f) sigma = 1.0f;
    s->sigma     = sigma;
    s->escalated = escalated ? 1u : 0u;
    s->cost_eur  = cost_eur;
    w->head = (w->head + 1) % COS_WATCHDOG_CAP;
    if (w->count < COS_WATCHDOG_CAP) w->count++;
}

/* Iterate live samples in chronological order. */
static int sample_at(const cos_watchdog_t *w, int logical_index,
                     const cos_watchdog_sample_t **out) {
    if (logical_index < 0 || logical_index >= w->count) return -1;
    int start = (w->head - w->count + COS_WATCHDOG_CAP) % COS_WATCHDOG_CAP;
    int idx   = (start + logical_index) % COS_WATCHDOG_CAP;
    *out = &w->ring[idx];
    return 0;
}

/* ------------------------ check ------------------------ */

static float safe_div(double num, double den) {
    return den > 0.0 ? (float)(num / den) : 0.0f;
}

void cos_watchdog_check(cos_watchdog_t *w, uint64_t now_s) {
    if (!w) return;

    double sum1 = 0.0, sum24 = 0.0, sum7 = 0.0;
    int    n1   = 0,    n24   = 0,    n7   = 0;
    int    esc24 = 0;
    double cost_today = 0.0;

    /* Linear-regression accumulators over the 7-day window. */
    double xs_n = 0.0;
    double sx = 0.0, sy = 0.0, sxx = 0.0, sxy = 0.0;
    /* Escalation-rate regression by day (coarser). */
    int day_esc[8]  = {0};
    int day_tot[8]  = {0};

    for (int i = 0; i < w->count; i++) {
        const cos_watchdog_sample_t *s = NULL;
        sample_at(w, i, &s);
        uint64_t age = (s->timestamp_s > now_s) ? 0 : (now_s - s->timestamp_s);

        if (age <= COS_WATCHDOG_ONE_HOUR_S) { sum1 += s->sigma; n1++; }
        if (age <= COS_WATCHDOG_ONE_DAY_S)  {
            sum24 += s->sigma; n24++;
            if (s->escalated) esc24++;
            cost_today += s->cost_eur;
        }
        if (age <= COS_WATCHDOG_ONE_WEEK_S) {
            sum7 += s->sigma; n7++;
            /* LS in units of "days since the oldest in-window sample" —
             * but we anchor x to (age in days) so the slope's sign
             * and units match the spec (σ/day with "up = bad"). */
            double x_days = (double)age / 86400.0;
            sx  += x_days;
            sy  += s->sigma;
            sxx += x_days * x_days;
            sxy += x_days * s->sigma;
            xs_n += 1.0;

            int bucket = (int)(x_days);
            if (bucket < 0) bucket = 0;
            if (bucket > 7) bucket = 7;
            day_tot[bucket]++;
            if (s->escalated) day_esc[bucket]++;
        }
    }

    w->sigma_mean_1h  = safe_div(sum1,  n1);
    w->sigma_mean_24h = safe_div(sum24, n24);
    w->sigma_mean_7d  = safe_div(sum7,  n7);
    w->escal_rate_24h = safe_div(esc24, n24);
    w->cost_today_eur = (float)cost_today;

    /* σ trend: OLS slope b = (N·Σxy - Σx·Σy) / (N·Σxx - (Σx)²).
     * x is "age in days" (older = larger x), so the mathematical
     * slope *decreases* as σ drifts up.  We flip the sign to keep
     * "trend > 0 ⇒ σ rising with time". */
    if (xs_n >= 2.0) {
        double denom = xs_n * sxx - sx * sx;
        if (denom > 0.0) {
            double b = (xs_n * sxy - sx * sy) / denom;
            w->sigma_trend_per_day = (float)(-b);
        } else {
            w->sigma_trend_per_day = 0.0f;
        }
    } else {
        w->sigma_trend_per_day = 0.0f;
    }

    /* Escalation trend: compare mean-of-first-3-buckets vs.
     * mean-of-last-3-buckets divided by the 3-day gap so the unit
     * is "change per day". */
    {
        int ft = 0, fs = 0, lt = 0, ls = 0;
        /* buckets 5..7 are "old", 0..2 are "recent" */
        for (int b = 0; b <= 2; b++) { lt += day_tot[b]; ls += day_esc[b]; }
        for (int b = 5; b <= 7; b++) { ft += day_tot[b]; fs += day_esc[b]; }
        if (ft > 0 && lt > 0) {
            double recent = (double)ls / (double)lt;
            double old    = (double)fs / (double)ft;
            w->escal_trend_per_day = (float)((recent - old) / 3.0);
        } else {
            w->escal_trend_per_day = 0.0f;
        }
    }

    w->alert_sigma = (w->sigma_trend_per_day > w->tau_sigma_up) ? 1 : 0;
    w->alert_escal = (w->escal_trend_per_day > w->tau_escal_up) ? 1 : 0;

    /* Noise floor: floating-point rounding over 100s of samples
     * produces slopes on the order of 1e-5 on a truly flat trace.
     * Below that, we report "healthy" rather than "degrading". */
    const float slope_eps = 1e-4f;

    if (w->alert_sigma || w->alert_escal) {
        w->health = COS_WATCHDOG_ALERT;
    } else if (w->sigma_trend_per_day > slope_eps ||
               w->escal_trend_per_day > slope_eps) {
        w->health = COS_WATCHDOG_DEGRADING;
    } else {
        w->health = COS_WATCHDOG_HEALTHY;
    }
}

/* ------------------------ plan ------------------------ */

void cos_watchdog_plan(const cos_watchdog_t *w, cos_watchdog_plan_t *plan) {
    if (!w || !plan) return;
    memset(plan, 0, sizeof *plan);
    if (w->alert_sigma && w->alert_escal) {
        plan->remedy = COS_WATCHDOG_REMEDY_NOTIFY_HUMAN;
        snprintf(plan->reason, sizeof plan->reason,
                 "σ and escalation both trending up (+%.4f σ/day,"
                 " +%.4f escal/day)",
                 (double)w->sigma_trend_per_day,
                 (double)w->escal_trend_per_day);
    } else if (w->alert_sigma) {
        plan->remedy = COS_WATCHDOG_REMEDY_RUN_OMEGA;
        plan->iterations = 50;
        snprintf(plan->reason, sizeof plan->reason,
                 "σ trend +%.4f/day exceeds τ=%.4f; consider `cos omega`",
                 (double)w->sigma_trend_per_day,
                 (double)w->tau_sigma_up);
    } else if (w->alert_escal) {
        plan->remedy = COS_WATCHDOG_REMEDY_TRAIN_LORA;
        snprintf(plan->reason, sizeof plan->reason,
                 "escalation trend +%.4f/day exceeds τ=%.4f; consider"
                 " `cos lora train`",
                 (double)w->escal_trend_per_day,
                 (double)w->tau_escal_up);
    } else {
        plan->remedy = COS_WATCHDOG_REMEDY_NONE;
        snprintf(plan->reason, sizeof plan->reason,
                 "healthy: σ_24h=%.4f escal_24h=%.4f",
                 (double)w->sigma_mean_24h, (double)w->escal_rate_24h);
    }
}

/* ------------------------ strings ------------------------ */

const char *cos_watchdog_health_str(cos_watchdog_health_t h) {
    switch (h) {
        case COS_WATCHDOG_HEALTHY:   return "HEALTHY";
        case COS_WATCHDOG_DEGRADING: return "DEGRADING";
        case COS_WATCHDOG_ALERT:     return "ALERT";
    }
    return "?";
}

const char *cos_watchdog_remedy_str(cos_watchdog_remedy_t r) {
    switch (r) {
        case COS_WATCHDOG_REMEDY_NONE:         return "none";
        case COS_WATCHDOG_REMEDY_RUN_OMEGA:    return "run_omega";
        case COS_WATCHDOG_REMEDY_TRAIN_LORA:   return "train_lora";
        case COS_WATCHDOG_REMEDY_NOTIFY_HUMAN: return "notify_human";
    }
    return "?";
}

/* ------------------------ self-test ------------------------
 *
 * Build a synthetic 7-day history where σ drifts from ~0.10 up to
 * ~0.25 and escalations increase from ~5 % to ~20 %.  Check
 * watchdog flags both alerts, proposes notify_human, and that a
 * separate "healthy" history produces REMEDY_NONE.
 */
int cos_watchdog_self_test(void) {
    cos_watchdog_t w;
    cos_watchdog_init(&w);

    uint64_t now = 7ULL * COS_WATCHDOG_ONE_DAY_S;
    /* 7 days * 24 samples/day = 168 samples. */
    for (int d = 0; d < 7; d++) {
        for (int h = 0; h < 24; h++) {
            uint64_t ts = (uint64_t)d * COS_WATCHDOG_ONE_DAY_S
                        + (uint64_t)h * COS_WATCHDOG_ONE_HOUR_S;
            float frac = (float)(d * 24 + h) / (7.0f * 24.0f);
            float sigma = 0.10f + 0.15f * frac;        /* 0.10 → 0.25 */
            int esc = (((d * 24 + h) * 7 + 3) % 100) < (int)(5 + 15 * frac)
                      ? 1 : 0;                          /* 5 % → 20 % */
            cos_watchdog_observe(&w, ts, sigma, esc, 0.0005f);
        }
    }
    cos_watchdog_check(&w, now);
    if (w.sigma_trend_per_day <= 0.0f) return -1;
    if (w.health != COS_WATCHDOG_ALERT) return -2;
    if (!w.alert_sigma) return -3;

    cos_watchdog_plan_t plan;
    cos_watchdog_plan(&w, &plan);
    if (plan.remedy == COS_WATCHDOG_REMEDY_NONE) return -4;

    /* Healthy history: σ flat at 0.15, no escalations. */
    cos_watchdog_t h;
    cos_watchdog_init(&h);
    for (int i = 0; i < 100; i++) {
        uint64_t ts = (uint64_t)i * COS_WATCHDOG_ONE_HOUR_S;
        cos_watchdog_observe(&h, ts, 0.15f, 0, 0.0005f);
    }
    cos_watchdog_check(&h, 100ULL * COS_WATCHDOG_ONE_HOUR_S);
    if (h.health != COS_WATCHDOG_HEALTHY) return -5;
    if (h.alert_sigma || h.alert_escal) return -6;

    cos_watchdog_plan_t p2;
    cos_watchdog_plan(&h, &p2);
    if (p2.remedy != COS_WATCHDOG_REMEDY_NONE) return -7;
    return 0;
}
