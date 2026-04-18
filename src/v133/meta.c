/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (c) 2026 Spektre Labs / Lauri Rainio
 *
 * v133 σ-Meta — implementation.
 */
#include "meta.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ==================================================================
 * History
 * ================================================================ */

void cos_v133_history_init(cos_v133_history_t *h) {
    if (!h) return;
    memset(h, 0, sizeof *h);
}

int cos_v133_history_append(cos_v133_history_t *h,
                            const cos_v133_snapshot_t *s) {
    if (!h || !s) return -1;
    if (h->n >= COS_V133_MAX_SNAPSHOTS) {
        memmove(h->snapshots, h->snapshots + 1,
                sizeof(cos_v133_snapshot_t)
                * (size_t)(COS_V133_MAX_SNAPSHOTS - 1));
        h->n = COS_V133_MAX_SNAPSHOTS - 1;
    }
    h->snapshots[h->n++] = *s;
    return 0;
}

/* ==================================================================
 * OLS slope helpers — fit y = a·x + b on (ts_unix, σ).
 *
 * We use the first snapshot's ts as origin to keep numerics tame.
 * Slope is returned per second; the public APIs convert to "per
 * week" for readability.
 * ================================================================ */

static float ols_slope_per_sec(const cos_v133_history_t *h, int ch) {
    if (!h || h->n < 2) return 0.0f;
    uint64_t origin = h->snapshots[0].ts_unix;
    double sx = 0, sy = 0, sxx = 0, sxy = 0;
    int n = 0;
    for (int i = 0; i < h->n; ++i) {
        const cos_v133_snapshot_t *s = &h->snapshots[i];
        double x = (double)(s->ts_unix - origin);
        double y = (ch < 0) ? (double)s->avg_sigma_product
                            : (double)s->avg_sigma_per_channel[ch];
        sx += x; sy += y; sxx += x * x; sxy += x * y;
        n++;
    }
    if (n < 2) return 0.0f;
    double denom = (double)n * sxx - sx * sx;
    if (fabs(denom) < 1e-12) return 0.0f;
    return (float)(((double)n * sxy - sx * sy) / denom);
}

float cos_v133_slope_per_week(const cos_v133_history_t *h) {
    return ols_slope_per_sec(h, -1) * 604800.0f;
}

float cos_v133_channel_slope_per_week(const cos_v133_history_t *h, int ch) {
    if (ch < 0 || ch >= COS_V133_CHANNELS) return 0.0f;
    return ols_slope_per_sec(h, ch) * 604800.0f;
}

/* ==================================================================
 * Meta-σ (coefficient of variation of σ_product)
 * ================================================================ */

float cos_v133_meta_sigma(const cos_v133_history_t *h) {
    if (!h || h->n < 2) return 0.0f;
    double m = 0.0;
    for (int i = 0; i < h->n; ++i)
        m += (double)h->snapshots[i].avg_sigma_product;
    m /= (double)h->n;
    if (m <= 1e-6) return 0.0f;
    double v = 0.0;
    for (int i = 0; i < h->n; ++i) {
        double d = (double)h->snapshots[i].avg_sigma_product - m;
        v += d * d;
    }
    v /= (double)h->n;
    return (float)(sqrt(v) / m);
}

/* ==================================================================
 * Regression verdict
 * ================================================================ */

cos_v133_verdict_t cos_v133_regression_verdict(const cos_v133_history_t *h,
                                               float tau_reg,
                                               float tau_meta) {
    if (!h || h->n < 2) return COS_V133_OK;
    float slope = cos_v133_slope_per_week(h);
    float meta  = cos_v133_meta_sigma(h);
    cos_v133_verdict_t v = COS_V133_OK;
    if (slope > tau_reg) v = COS_V133_REGRESSION_DETECTED;
    if (meta  > tau_meta) v = COS_V133_CALIBRATION_DRIFT;
    return v;
}

/* ==================================================================
 * Auto-diagnose: highest-σ channel → cause label
 * ================================================================ */

cos_v133_cause_t cos_v133_diagnose(const cos_v133_snapshot_t *s,
                                   float spike_tau) {
    if (!s) return COS_V133_CAUSE_NONE;
    int   best = -1;
    float best_v = spike_tau;   /* must exceed threshold */
    for (int i = 0; i < COS_V133_CHANNELS; ++i) {
        if (s->avg_sigma_per_channel[i] > best_v) {
            best_v = s->avg_sigma_per_channel[i];
            best   = i;
        }
    }
    if (best < 0) return COS_V133_CAUSE_NONE;
    switch (best) {
    case 0: return COS_V133_CAUSE_ADAPTER_CORRUPTED;
    case 1: return COS_V133_CAUSE_KV_CACHE_DEGENERATE;
    case 2: return COS_V133_CAUSE_MEMORY_POLLUTED;
    case 3: return COS_V133_CAUSE_TOOL_SIGMA_SPIKE;
    case 4: return COS_V133_CAUSE_PLAN_SIGMA_SPIKE;
    case 5: return COS_V133_CAUSE_VISION_SIGMA_SPIKE;
    case 6: return COS_V133_CAUSE_RED_TEAM_FAIL;
    case 7: return COS_V133_CAUSE_FORMAL_VIOLATION;
    default: return COS_V133_CAUSE_UNKNOWN;
    }
}

const char *cos_v133_cause_label(cos_v133_cause_t c) {
    switch (c) {
    case COS_V133_CAUSE_NONE:                return "none";
    case COS_V133_CAUSE_ADAPTER_CORRUPTED:   return "adapter_corrupted";
    case COS_V133_CAUSE_KV_CACHE_DEGENERATE: return "kv_cache_degenerate";
    case COS_V133_CAUSE_MEMORY_POLLUTED:     return "memory_polluted";
    case COS_V133_CAUSE_TOOL_SIGMA_SPIKE:    return "tool_sigma_spike";
    case COS_V133_CAUSE_PLAN_SIGMA_SPIKE:    return "plan_sigma_spike";
    case COS_V133_CAUSE_VISION_SIGMA_SPIKE:  return "vision_sigma_spike";
    case COS_V133_CAUSE_RED_TEAM_FAIL:       return "red_team_fail";
    case COS_V133_CAUSE_FORMAL_VIOLATION:    return "formal_violation";
    default:                                  return "unknown";
    }
}

static const char *verdict_label(cos_v133_verdict_t v) {
    switch (v) {
    case COS_V133_OK:                   return "ok";
    case COS_V133_REGRESSION_DETECTED:  return "regression_detected";
    case COS_V133_CALIBRATION_DRIFT:    return "calibration_drift";
    }
    return "unknown";
}

/* ==================================================================
 * Self-benchmark runner
 *
 * Compares a normalized form of expected and produced answers —
 * case-insensitive, leading/trailing whitespace stripped, trailing
 * punctuation removed.  "correct" is substring-or-equality.
 * ================================================================ */

static void norm(const char *in, char *out, size_t cap) {
    size_t n = 0;
    while (*in && (unsigned char)*in <= 0x20) in++;      /* ltrim */
    while (*in && n + 1 < cap) {
        unsigned char c = (unsigned char)*in++;
        if (c >= 'A' && c <= 'Z') c = (unsigned char)(c + 32);
        out[n++] = (char)c;
    }
    while (n > 0) {
        unsigned char c = (unsigned char)out[n - 1];
        if (c == '.' || c == '!' || c == '?' || c == ',' || c <= 0x20) n--;
        else break;
    }
    out[n] = '\0';
}

int cos_v133_run_benchmark(cos_v133_answer_fn fn, void *ctx,
                           const char **questions,
                           const char **expected,
                           int n,
                           cos_v133_bench_result_t *out) {
    if (!fn || !questions || !expected || n <= 0 || !out) return -1;
    memset(out, 0, sizeof *out);
    out->n_questions = n;

    double sigma_sum = 0.0;
    int correct = 0;
    for (int i = 0; i < n; ++i) {
        char  resp [512]; resp [0] = '\0';
        float sig = 0.0f;
        int   rc = fn(questions[i], resp, sizeof resp, &sig, ctx);
        if (rc < 0) continue;
        sigma_sum += sig;

        char a[512]; char b[512];
        norm(resp,        a, sizeof a);
        norm(expected[i], b, sizeof b);
        if (!strcmp(a, b) || (b[0] && strstr(a, b) != NULL)) correct++;
    }
    out->n_correct          = correct;
    out->accuracy           = (float)correct / (float)n;
    out->mean_sigma_product = (float)(sigma_sum / (double)n);
    return 0;
}

/* ==================================================================
 * Composite health + JSON
 * ================================================================ */

int cos_v133_health_compose(const cos_v133_history_t *h,
                            float tau_reg, float tau_meta,
                            float spike_tau,
                            cos_v133_health_t *out) {
    if (!h || !out) return -1;
    memset(out, 0, sizeof *out);
    out->n_snapshots = h->n;
    if (h->n == 0) return 0;
    out->latest = h->snapshots[h->n - 1];
    out->slope_per_week = cos_v133_slope_per_week(h);
    out->meta_sigma     = cos_v133_meta_sigma(h);
    out->verdict        = cos_v133_regression_verdict(h, tau_reg, tau_meta);
    out->cause          = cos_v133_diagnose(&out->latest, spike_tau);
    return 0;
}

int cos_v133_health_to_json(const cos_v133_health_t *h, char *out, size_t cap) {
    if (!h || !out || cap == 0) return -1;
    int off = snprintf(out, cap,
        "{\"n_snapshots\":%d,\"slope_per_week\":%.6f,"
        "\"avg_sigma_product\":%.4f,\"meta_sigma\":%.4f,"
        "\"verdict\":\"%s\",\"cause\":\"%s\","
        "\"channels\":[",
        h->n_snapshots, (double)h->slope_per_week,
        (double)h->latest.avg_sigma_product,
        (double)h->meta_sigma,
        verdict_label(h->verdict),
        cos_v133_cause_label(h->cause));
    if (off < 0 || (size_t)off >= cap) return -1;
    for (int i = 0; i < COS_V133_CHANNELS; ++i) {
        int w = snprintf(out + off, cap - (size_t)off, "%s%.4f",
            i == 0 ? "" : ",",
            (double)h->latest.avg_sigma_per_channel[i]);
        if (w < 0 || (size_t)(off + w) >= cap) return -1;
        off += w;
    }
    int w = snprintf(out + off, cap - (size_t)off, "]}");
    if (w < 0 || (size_t)(off + w) >= cap) return -1;
    return off + w;
}

/* ==================================================================
 * Self-test
 * ================================================================ */

#define _CHECK(cond, msg) do { \
    if (!(cond)) { \
        fprintf(stderr, "v133 self-test FAIL: %s (line %d)\n", (msg), __LINE__); \
        return 1; \
    } \
} while (0)

/* Mock answer function: returns the expected answer with fixed σ
 * unless the question contains "fail" (wrong answer) or "hard"
 * (correct but high σ). */
static int mock_answer(const char *q, char *out, size_t cap,
                       float *sig, void *ctx) {
    const char **expected = (const char**)ctx;
    static int idx = 0;
    const char *ans = expected[idx];
    if (strstr(q, "fail")) { ans = "NOPE"; *sig = 0.80f; }
    else if (strstr(q, "hard")) {
        *sig = 0.70f;
    } else {
        *sig = 0.15f;
    }
    snprintf(out, cap, "%s", ans);
    idx = (idx + 1) % 4;   /* 4-question test */
    return 0;
}

int cos_v133_self_test(void) {
    /* --- history + flat σ → no regression, low meta-σ --------- */
    fprintf(stderr, "check-v133: stable history → no regression\n");
    cos_v133_history_t h;
    cos_v133_history_init(&h);
    uint64_t base = 1700000000ULL;
    for (int i = 0; i < 8; ++i) {
        cos_v133_snapshot_t s = {0};
        s.ts_unix = base + (uint64_t)i * 604800ULL;
        s.avg_sigma_product = 0.30f;
        for (int c = 0; c < COS_V133_CHANNELS; ++c)
            s.avg_sigma_per_channel[c] = 0.30f;
        s.n_interactions = 100;
        cos_v133_history_append(&h, &s);
    }
    cos_v133_verdict_t v = cos_v133_regression_verdict(
        &h, COS_V133_REGRESSION_TAU_DEFAULT, COS_V133_META_TAU_DEFAULT);
    _CHECK(v == COS_V133_OK, "stable → ok");
    float meta = cos_v133_meta_sigma(&h);
    _CHECK(meta < 0.05f, "stable meta-σ near zero");

    /* --- rising σ → regression ---------------------------------- */
    fprintf(stderr, "check-v133: rising σ → regression detected\n");
    cos_v133_history_init(&h);
    for (int i = 0; i < 8; ++i) {
        cos_v133_snapshot_t s = {0};
        s.ts_unix = base + (uint64_t)i * 604800ULL;
        s.avg_sigma_product = 0.30f + (float)i * 0.03f;
        for (int c = 0; c < COS_V133_CHANNELS; ++c)
            s.avg_sigma_per_channel[c] = s.avg_sigma_product;
        cos_v133_history_append(&h, &s);
    }
    float slope = cos_v133_slope_per_week(&h);
    fprintf(stderr, "  slope/week = %.6f\n", (double)slope);
    _CHECK(slope > 0.025f, "positive slope per week");
    v = cos_v133_regression_verdict(
        &h, COS_V133_REGRESSION_TAU_DEFAULT, 1.0f /* no calibration cap */);
    _CHECK(v == COS_V133_REGRESSION_DETECTED, "regression flagged");

    /* --- noisy σ → calibration drift ---------------------------- */
    fprintf(stderr, "check-v133: noisy σ → calibration drift\n");
    cos_v133_history_init(&h);
    float noisy[] = { 0.10f, 0.80f, 0.12f, 0.90f, 0.15f, 0.85f, 0.11f, 0.79f };
    for (int i = 0; i < 8; ++i) {
        cos_v133_snapshot_t s = {0};
        s.ts_unix = base + (uint64_t)i * 604800ULL;
        s.avg_sigma_product = noisy[i];
        for (int c = 0; c < COS_V133_CHANNELS; ++c)
            s.avg_sigma_per_channel[c] = noisy[i];
        cos_v133_history_append(&h, &s);
    }
    meta = cos_v133_meta_sigma(&h);
    fprintf(stderr, "  meta-σ = %.4f\n", (double)meta);
    _CHECK(meta > COS_V133_META_TAU_DEFAULT, "meta-σ > τ_meta");
    v = cos_v133_regression_verdict(
        &h, COS_V133_REGRESSION_TAU_DEFAULT, COS_V133_META_TAU_DEFAULT);
    _CHECK(v == COS_V133_CALIBRATION_DRIFT, "calibration drift flagged");

    /* --- diagnose: highest channel wins ------------------------- */
    fprintf(stderr, "check-v133: diagnose\n");
    cos_v133_snapshot_t d = {0};
    for (int c = 0; c < COS_V133_CHANNELS; ++c) d.avg_sigma_per_channel[c] = 0.15f;
    d.avg_sigma_per_channel[1] = 0.80f;  /* kv-cache */
    cos_v133_cause_t c = cos_v133_diagnose(&d, 0.40f);
    _CHECK(c == COS_V133_CAUSE_KV_CACHE_DEGENERATE, "kv channel wins");
    d.avg_sigma_per_channel[6] = 0.95f;  /* red-team — even higher */
    c = cos_v133_diagnose(&d, 0.40f);
    _CHECK(c == COS_V133_CAUSE_RED_TEAM_FAIL, "red-team dominates");
    d.avg_sigma_per_channel[1] = 0.10f;
    d.avg_sigma_per_channel[6] = 0.10f;
    c = cos_v133_diagnose(&d, 0.40f);
    _CHECK(c == COS_V133_CAUSE_NONE, "no channel above threshold");

    /* --- self-benchmark runner ---------------------------------- */
    fprintf(stderr, "check-v133: self-benchmark runner\n");
    const char *questions[4] = {
        "capital of France?",
        "2+2 easy?",
        "hard quantum question?",
        "fail this one please?",
    };
    const char *expected[4] = { "paris", "4", "superposition", "42" };
    cos_v133_bench_result_t br;
    int rc = cos_v133_run_benchmark(mock_answer, (void*)expected,
                                    questions, expected, 4, &br);
    _CHECK(rc == 0, "bench rc");
    _CHECK(br.n_questions == 4, "n_questions");
    _CHECK(br.n_correct == 3,   "3/4 correct (1 'fail' wrong)");
    _CHECK(br.mean_sigma_product > 0.20f
        && br.mean_sigma_product < 0.60f, "mean σ reasonable");

    /* --- composite health JSON ---------------------------------- */
    cos_v133_health_t hl;
    cos_v133_health_compose(&h, COS_V133_REGRESSION_TAU_DEFAULT,
                            COS_V133_META_TAU_DEFAULT, 0.40f, &hl);
    char js[1024];
    int wsz = cos_v133_health_to_json(&hl, js, sizeof js);
    _CHECK(wsz > 0 && wsz < (int)sizeof js, "json fit");
    _CHECK(strstr(js, "\"verdict\":\"calibration_drift\"") != NULL,
           "verdict label present");

    fprintf(stderr, "check-v133: OK (history + slope + meta-σ + diagnose + bench)\n");
    return 0;
}
