/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (c) 2026 Spektre Labs / Lauri Rainio
 *
 * v131 σ-Temporal — implementation.
 */
#include "temporal.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int cos_v131_timeline_init(cos_v131_timeline_t *t, int cap) {
    if (!t || cap <= 0) return -1;
    memset(t, 0, sizeof *t);
    t->events = (cos_v131_event_t*)calloc((size_t)cap, sizeof(cos_v131_event_t));
    if (!t->events) return -1;
    t->cap = cap;
    t->n   = 0;
    return 0;
}

void cos_v131_timeline_free(cos_v131_timeline_t *t) {
    if (!t) return;
    free(t->events);
    t->events = NULL;
    t->n = t->cap = 0;
}

int cos_v131_timeline_append(cos_v131_timeline_t *t,
                             const cos_v131_event_t *e) {
    if (!t || !e) return -1;
    if (t->n >= t->cap) {
        /* Drop oldest on overflow (ring-like, preserves recency). */
        memmove(t->events, t->events + 1,
                sizeof(cos_v131_event_t) * (size_t)(t->cap - 1));
        t->n = t->cap - 1;
    }
    t->events[t->n++] = *e;
    return 0;
}

int cos_v131_recall_window(const cos_v131_timeline_t *t,
                           uint64_t since, uint64_t until,
                           const char *topic,
                           const cos_v131_event_t **out, int max) {
    if (!t || !out || max <= 0) return 0;
    int filled = 0;
    for (int i = 0; i < t->n && filled < max; ++i) {
        const cos_v131_event_t *e = &t->events[i];
        if (e->ts_unix < since) continue;
        if (e->ts_unix > until) continue;
        if (topic && topic[0] && strcmp(e->topic, topic) != 0) continue;
        out[filled++] = e;
    }
    return filled;
}

float cos_v131_sigma_trend(const cos_v131_timeline_t *t, const char *topic,
                           uint64_t since, uint64_t until,
                           int *n_used_out) {
    if (!t) { if (n_used_out) *n_used_out = 0; return 0.0f; }
    /* OLS slope on the matching subset. */
    double sx = 0, sy = 0, sxx = 0, sxy = 0;
    int n = 0;
    /* Use first matching event's ts as origin to keep numerics sane. */
    uint64_t origin = 0;
    int have_origin = 0;
    for (int i = 0; i < t->n; ++i) {
        const cos_v131_event_t *e = &t->events[i];
        if (e->ts_unix < since || e->ts_unix > until) continue;
        if (topic && topic[0] && strcmp(e->topic, topic) != 0) continue;
        if (!have_origin) { origin = e->ts_unix; have_origin = 1; }
        double x = (double)(e->ts_unix - origin);
        double y = (double)e->sigma_product;
        sx += x; sy += y; sxx += x * x; sxy += x * y;
        n++;
    }
    if (n_used_out) *n_used_out = n;
    if (n < 2) return 0.0f;
    double denom = (double)n * sxx - sx * sx;
    if (fabs(denom) < 1e-12) return 0.0f;
    return (float)(((double)n * sxy - sx * sy) / denom);
}

float cos_v131_decay_weight(uint64_t age_seconds, float sigma_product,
                            uint64_t half_life_seconds, float alpha) {
    if (half_life_seconds == 0) return 1.0f;
    float s = sigma_product < 0 ? 0 : (sigma_product > 1 ? 1 : sigma_product);
    /* exp(-age/half_life · (1 + α·σ)).  "Effective half-life" shrinks
     * with σ → uncertain memories decay faster. */
    double k = (double)age_seconds / (double)half_life_seconds;
    double eff = k * (1.0 + (double)alpha * (double)s);
    return (float)exp(-eff);
}

int cos_v131_detect_spikes(const cos_v131_timeline_t *t,
                           const char *topic,
                           float threshold,
                           int *out_idx, int max) {
    if (!t || !out_idx || max <= 0) return 0;
    int filled = 0;
    int prev = -1;
    for (int i = 0; i < t->n && filled < max; ++i) {
        const cos_v131_event_t *e = &t->events[i];
        if (topic && topic[0] && strcmp(e->topic, topic) != 0) continue;
        if (prev >= 0) {
            float d = e->sigma_product - t->events[prev].sigma_product;
            if (d >= threshold) out_idx[filled++] = i;
        }
        prev = i;
    }
    return filled;
}

float cos_v131_predict_sigma_under_pressure(
    const cos_v131_deadline_sigma_t *ds, float f) {
    if (!ds) return 0.0f;
    if (f < 0.0f) f = 0.0f;
    if (f > 1.0f) f = 1.0f;
    float s = ds->baseline_sigma + ds->pressure_slope * f;
    if (s < 0.0f) s = 0.0f;
    if (s > 1.0f) s = 1.0f;
    return s;
}

int cos_v131_trend_to_json(const char *topic, float slope,
                           int n_used, char *out, size_t cap) {
    if (!out || cap == 0) return -1;
    return snprintf(out, cap,
        "{\"topic\":\"%s\",\"slope_per_sec\":%.8f,\"slope_per_day\":%.6f,"
        "\"n_used\":%d}",
        topic ? topic : "",
        (double)slope, (double)slope * 86400.0, n_used);
}

/* ==================================================================
 * Self-test
 * ================================================================ */

#define _CHECK(cond, msg) do { \
    if (!(cond)) { \
        fprintf(stderr, "v131 self-test FAIL: %s (line %d)\n", (msg), __LINE__); \
        cos_v131_timeline_free(&tl); \
        return 1; \
    } \
} while (0)

int cos_v131_self_test(void) {
    cos_v131_timeline_t tl;
    _CHECK(cos_v131_timeline_init(&tl, 64) == 0, "init");

    /* Synthesise 14 events over two weeks.  math σ trends DOWN
     * (learning), biology σ trends UP (regression). */
    uint64_t base = 1700000000ULL;  /* fixed epoch seed */
    for (int i = 0; i < 14; ++i) {
        cos_v131_event_t e = {0};
        e.ts_unix = base + (uint64_t)i * 86400ULL;  /* daily */
        if (i % 2 == 0) {
            strncpy(e.topic, "math", sizeof e.topic - 1);
            e.sigma_product = 0.80f - (float)i * 0.05f;
            if (e.sigma_product < 0.05f) e.sigma_product = 0.05f;
        } else {
            strncpy(e.topic, "biology", sizeof e.topic - 1);
            e.sigma_product = 0.20f + (float)i * 0.03f;
            if (e.sigma_product > 0.95f) e.sigma_product = 0.95f;
        }
        snprintf(e.summary, sizeof e.summary, "event %d", i);
        _CHECK(cos_v131_timeline_append(&tl, &e) == 0, "append");
    }

    /* --- window recall ------------------------------------------ */
    fprintf(stderr, "check-v131: window recall\n");
    const cos_v131_event_t *hits[32];
    int n = cos_v131_recall_window(&tl, base, base + 7 * 86400ULL,
                                    NULL, hits, 32);
    _CHECK(n == 8, "8 events in first week (inclusive endpoints)");
    n = cos_v131_recall_window(&tl, base, base + 14 * 86400ULL,
                               "math", hits, 32);
    _CHECK(n == 7, "7 math events in 14 days");

    /* --- σ-trend: math decreases, biology increases ------------- */
    fprintf(stderr, "check-v131: σ-trend slope signs\n");
    int used_m = 0, used_b = 0;
    float slope_math = cos_v131_sigma_trend(&tl, "math",
                                            base, base + 14 * 86400ULL, &used_m);
    float slope_bio  = cos_v131_sigma_trend(&tl, "biology",
                                            base, base + 14 * 86400ULL, &used_b);
    fprintf(stderr, "  math slope=%.8g (%d pts), biology slope=%.8g (%d pts)\n",
            slope_math, used_m, slope_bio, used_b);
    _CHECK(slope_math < 0.0f,     "math slope negative (learning)");
    _CHECK(slope_bio  > 0.0f,     "biology slope positive (regression)");
    _CHECK(used_m == 7 && used_b == 7, "sample counts");

    /* --- decay weights ------------------------------------------ */
    fprintf(stderr, "check-v131: σ-weighted decay\n");
    uint64_t hl = 7 * 86400UL;
    float w_fresh_confident = cos_v131_decay_weight(0,         0.10f, hl, 1.0f);
    float w_fresh_uncertain = cos_v131_decay_weight(0,         0.90f, hl, 1.0f);
    float w_old_confident   = cos_v131_decay_weight(30*86400,  0.10f, hl, 1.0f);
    float w_old_uncertain   = cos_v131_decay_weight(30*86400,  0.90f, hl, 1.0f);
    _CHECK(fabsf(w_fresh_confident - 1.0f) < 1e-4f,
                                                   "fresh weight == 1");
    _CHECK(fabsf(w_fresh_uncertain - 1.0f) < 1e-4f,
                                                   "fresh σ-independent");
    _CHECK(w_old_confident > w_old_uncertain,
                                                   "old+uncertain decays faster");
    _CHECK(w_old_uncertain < 0.10f,
                                                   "30d + high σ severely decayed");

    /* --- spikes -------------------------------------------------- */
    fprintf(stderr, "check-v131: spike detection\n");
    /* Inject a deliberate spike after index 14. */
    cos_v131_event_t spike = {0};
    spike.ts_unix = base + 20 * 86400UL;
    strncpy(spike.topic, "math", sizeof spike.topic - 1);
    spike.sigma_product = 0.90f;   /* jumps from very low to very high */
    cos_v131_timeline_append(&tl, &spike);
    int idx[8];
    int n_sp = cos_v131_detect_spikes(&tl, "math", 0.40f, idx, 8);
    _CHECK(n_sp >= 1, "spike detected on math topic");

    /* --- deadline σ -------------------------------------------- */
    fprintf(stderr, "check-v131: deadline σ prediction\n");
    cos_v131_deadline_sigma_t ds = { .baseline_sigma = 0.20f,
                                     .pressure_slope = 0.60f };
    float s_early = cos_v131_predict_sigma_under_pressure(&ds, 0.10f);
    float s_late  = cos_v131_predict_sigma_under_pressure(&ds, 0.95f);
    _CHECK(s_late > s_early, "σ rises under deadline pressure");
    _CHECK(s_late  <= 1.0f && s_early >= 0.0f, "σ in [0,1]");

    /* --- JSON shape --------------------------------------------- */
    fprintf(stderr, "check-v131: trend JSON shape\n");
    char js[256];
    _CHECK(cos_v131_trend_to_json("math", slope_math, used_m, js, sizeof js) > 0,
           "json write");
    _CHECK(strstr(js, "\"topic\":\"math\"") != NULL, "json topic");

    cos_v131_timeline_free(&tl);
    fprintf(stderr, "check-v131: OK (timeline + trend + decay + spike + deadline)\n");
    return 0;
}
