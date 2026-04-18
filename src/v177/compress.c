/*
 * v177 σ-Compress — implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "compress.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static uint64_t splitmix64(uint64_t *s) {
    uint64_t z = (*s += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

static float frand(uint64_t *s) {
    return (splitmix64(s) >> 40) / 16777216.0f;
}

static float clampf(float x, float lo, float hi) {
    if (x < lo) return lo;
    if (x > hi) return hi;
    return x;
}

void cos_v177_init(cos_v177_state_t *s, uint64_t seed) {
    memset(s, 0, sizeof(*s));
    s->seed             = seed ? seed : 0x177C0FFEE177ULL;
    s->prune_tau        = 0.05f;     /* σ_impact < 5 % → safe to prune */
    s->int8_tau         = 0.15f;     /* low σ → keep INT8 */
    s->int4_tau         = 0.40f;     /* mid σ → INT4 */
    /* above int4_tau → INT2 */
    s->merge_tau        = 0.03f;     /* |Δσ_layer| ≤ 3 % ⇒ merge */
    s->drift_budget_pct = 5.0f;
}

/* ------------------------------------------------------------- */
/* Fixture: 16 layers × 64 neurons                               */
/* ------------------------------------------------------------- */

void cos_v177_build_fixture(cos_v177_state_t *s) {
    /* σ_layer bands (hand-tuned for testability):
     *
     *   L  0- 4  (low σ)   → ~0.10     → INT8
     *   L  5- 9  (mid σ)   → ~0.30     → INT4
     *   L 10-15  (high σ)  → ~0.55     → INT2
     *
     * A few adjacent pairs within each band have near-identical
     * σ to trigger the merge pass.
     */
    static const float LAYER_SIGMA[COS_V177_N_LAYERS] = {
        0.080f, 0.081f, 0.110f, 0.112f, 0.140f,   /* INT8 band */
        0.260f, 0.261f, 0.300f, 0.340f, 0.341f,   /* INT4 band */
        0.470f, 0.471f, 0.500f, 0.540f, 0.541f, 0.600f, /* INT2 band */
    };
    uint64_t rs = s->seed ^ 0xFACADE177ULL;
    for (int L = 0; L < COS_V177_N_LAYERS; ++L) {
        cos_v177_layer_t *layer = &s->layers[L];
        memset(layer, 0, sizeof(*layer));
        layer->id        = L;
        layer->precision = COS_V177_PREC_INT8;
        layer->n_alive   = COS_V177_N_NEURONS;

        /* 70 % of neurons prunable (σ_impact < 0.05);
         * 30 % critical (σ_impact in [0.10, 0.90]). */
        for (int n = 0; n < COS_V177_N_NEURONS; ++n) {
            float roll = frand(&rs);
            float sig;
            if (roll < 0.70f) {
                sig = 0.001f + 0.04f * frand(&rs);
            } else {
                sig = 0.10f + 0.80f * frand(&rs);
            }
            layer->sigma_impact[n] = clampf(sig, 0.0f, 1.0f);
            layer->pruned[n]       = false;
        }
        layer->sigma_layer = LAYER_SIGMA[L];
    }
}

/* ------------------------------------------------------------- */
/* Pruning                                                       */
/* ------------------------------------------------------------- */

void cos_v177_prune(cos_v177_state_t *s) {
    for (int L = 0; L < COS_V177_N_LAYERS; ++L) {
        cos_v177_layer_t *layer = &s->layers[L];
        int alive = 0;
        double sum_pruned_impact = 0.0;
        double sum_total_impact  = 0.0;
        for (int n = 0; n < COS_V177_N_NEURONS; ++n) {
            sum_total_impact += layer->sigma_impact[n];
            if (layer->sigma_impact[n] < s->prune_tau) {
                layer->pruned[n]    = true;
                sum_pruned_impact  += layer->sigma_impact[n];
            } else {
                layer->pruned[n]    = false;
                alive++;
            }
        }
        layer->n_alive = alive;
        /* Proportional drift: the σ we lost by pruning relative to
         * the layer's total σ-impact.  The σ_impact of pruned
         * neurons is bounded (all < prune_tau), so the nudge is
         * bounded and never exceeds a few percent of σ_layer. */
        if (sum_total_impact > 0.0) {
            double frac = sum_pruned_impact / sum_total_impact;
            layer->sigma_layer = clampf(
                layer->sigma_layer + (float)(frac * 0.02), 0.0f, 1.0f);
        }
    }
}

/* ------------------------------------------------------------- */
/* Mixed precision                                               */
/* ------------------------------------------------------------- */

void cos_v177_mixed_precision(cos_v177_state_t *s) {
    for (int L = 0; L < COS_V177_N_LAYERS; ++L) {
        cos_v177_layer_t *layer = &s->layers[L];
        if (layer->sigma_layer <= s->int8_tau)
            layer->precision = COS_V177_PREC_INT8;
        else if (layer->sigma_layer <= s->int4_tau)
            layer->precision = COS_V177_PREC_INT4;
        else
            layer->precision = COS_V177_PREC_INT2;
    }
}

/* ------------------------------------------------------------- */
/* Layer merging                                                 */
/* ------------------------------------------------------------- */

void cos_v177_merge_layers(cos_v177_state_t *s) {
    for (int L = 1; L < COS_V177_N_LAYERS; ++L) {
        cos_v177_layer_t *prev = &s->layers[L - 1];
        cos_v177_layer_t *cur  = &s->layers[L];
        if (prev->merged_into_prev) continue;   /* keep chains short */
        float d = fabsf(cur->sigma_layer - prev->sigma_layer);
        if (d <= s->merge_tau && cur->precision == prev->precision) {
            cur->merged_into_prev = true;
        }
    }
}

/* ------------------------------------------------------------- */
/* Report                                                        */
/* ------------------------------------------------------------- */

static float sigma_calibration(const cos_v177_state_t *s) {
    /* Calibration σ = mean over layer σ_layer weighted by
     * precision cost (INT8 contributes 1.0; INT4 0.9; INT2 0.8
     * because more quantisation risks calibration drift).
     * Merged layers count once. */
    double sum = 0.0;
    double w   = 0.0;
    for (int L = 0; L < COS_V177_N_LAYERS; ++L) {
        const cos_v177_layer_t *layer = &s->layers[L];
        if (layer->merged_into_prev) continue;
        double weight;
        switch (layer->precision) {
            case COS_V177_PREC_INT8: weight = 1.00; break;
            case COS_V177_PREC_INT4: weight = 0.90; break;
            case COS_V177_PREC_INT2: weight = 0.80; break;
            default:                  weight = 1.00; break;
        }
        sum += weight * layer->sigma_layer;
        w   += weight;
    }
    return (float)(sum / (w > 0.0 ? w : 1.0));
}

static float sigma_baseline(const cos_v177_state_t *s) {
    /* Baseline σ before any compression: unweighted mean of σ_layer
     * using the uncompressed state's layer bias only. */
    double sum = 0.0;
    for (int L = 0; L < COS_V177_N_LAYERS; ++L)
        sum += s->layers[L].sigma_layer;
    return (float)(sum / COS_V177_N_LAYERS);
}

void cos_v177_report(cos_v177_state_t *s) {
    /* Baseline is computed on a *separate* untouched state so we
     * can measure drift honestly. */
    cos_v177_state_t base;
    cos_v177_init(&base, s->seed);
    cos_v177_build_fixture(&base);

    s->report.params_before   = COS_V177_N_LAYERS * COS_V177_N_NEURONS;
    s->report.bytes_before    = s->report.params_before; /* INT8 = 1 B */
    s->report.sigma_before    = sigma_baseline(&base);
    s->report.n_layers_before = COS_V177_N_LAYERS;

    size_t params_after = 0;
    size_t bytes_after  = 0;
    int    pruned = 0, i8 = 0, i4 = 0, i2 = 0;
    int    merged = 0, alive_layers = 0;
    for (int L = 0; L < COS_V177_N_LAYERS; ++L) {
        const cos_v177_layer_t *layer = &s->layers[L];
        if (layer->merged_into_prev) { merged++; continue; }
        alive_layers++;
        params_after += layer->n_alive;
        double bytes_per_param = 1.0;
        switch (layer->precision) {
            case COS_V177_PREC_INT8: bytes_per_param = 1.0;  i8++; break;
            case COS_V177_PREC_INT4: bytes_per_param = 0.5;  i4++; break;
            case COS_V177_PREC_INT2: bytes_per_param = 0.25; i2++; break;
        }
        bytes_after += (size_t)((double)layer->n_alive * bytes_per_param);
        pruned += COS_V177_N_NEURONS - layer->n_alive;
    }
    /* neurons inside merged layers are subsumed by the parent */
    s->report.params_after      = params_after;
    s->report.bytes_after       = bytes_after;
    s->report.sigma_after       = sigma_calibration(s);
    s->report.sigma_drift_pct   = (s->report.sigma_before > 0.0f)
        ? 100.0f * fabsf(s->report.sigma_after - s->report.sigma_before) /
                   s->report.sigma_before
        : 0.0f;
    s->report.n_layers_after    = alive_layers;
    s->report.n_neurons_pruned  = pruned;
    s->report.n_layers_int8     = i8;
    s->report.n_layers_int4     = i4;
    s->report.n_layers_int2     = i2;
    s->report.n_layers_merged   = merged;
}

/* ------------------------------------------------------------- */
/* JSON                                                          */
/* ------------------------------------------------------------- */

static const char *prec_name(cos_v177_prec_t p) {
    switch (p) {
        case COS_V177_PREC_INT8: return "INT8";
        case COS_V177_PREC_INT4: return "INT4";
        case COS_V177_PREC_INT2: return "INT2";
    }
    return "?";
}

size_t cos_v177_to_json(const cos_v177_state_t *s, char *buf, size_t cap) {
    if (!s || !buf) return 0;
    size_t used = 0;
    const cos_v177_report_t *r = &s->report;
    int n = snprintf(buf + used, cap - used,
        "{\"kernel\":\"v177\",\"prune_tau\":%.4f,\"int8_tau\":%.4f,"
        "\"int4_tau\":%.4f,\"merge_tau\":%.4f,"
        "\"drift_budget_pct\":%.2f,\"report\":{"
        "\"params_before\":%zu,\"params_after\":%zu,"
        "\"bytes_before\":%zu,\"bytes_after\":%zu,"
        "\"sigma_before\":%.4f,\"sigma_after\":%.4f,"
        "\"sigma_drift_pct\":%.4f,"
        "\"n_layers_before\":%d,\"n_layers_after\":%d,"
        "\"n_neurons_pruned\":%d,\"n_layers_int8\":%d,"
        "\"n_layers_int4\":%d,\"n_layers_int2\":%d,"
        "\"n_layers_merged\":%d},\"layers\":[",
        (double)s->prune_tau, (double)s->int8_tau,
        (double)s->int4_tau, (double)s->merge_tau,
        (double)s->drift_budget_pct,
        r->params_before, r->params_after,
        r->bytes_before,  r->bytes_after,
        (double)r->sigma_before, (double)r->sigma_after,
        (double)r->sigma_drift_pct,
        r->n_layers_before, r->n_layers_after,
        r->n_neurons_pruned, r->n_layers_int8,
        r->n_layers_int4, r->n_layers_int2,
        r->n_layers_merged);
    if (n < 0 || (size_t)n >= cap - used) return 0;
    used += (size_t)n;
    for (int L = 0; L < COS_V177_N_LAYERS; ++L) {
        const cos_v177_layer_t *layer = &s->layers[L];
        n = snprintf(buf + used, cap - used,
            "%s{\"id\":%d,\"n_alive\":%d,\"sigma_layer\":%.4f,"
            "\"precision\":\"%s\",\"merged\":%s}",
            L == 0 ? "" : ",", layer->id, layer->n_alive,
            (double)layer->sigma_layer,
            prec_name(layer->precision),
            layer->merged_into_prev ? "true" : "false");
        if (n < 0 || (size_t)n >= cap - used) return 0;
        used += (size_t)n;
    }
    n = snprintf(buf + used, cap - used, "]}");
    if (n < 0 || (size_t)n >= cap - used) return 0;
    used += (size_t)n;
    return used;
}

/* ------------------------------------------------------------- */
/* Self-test                                                     */
/* ------------------------------------------------------------- */

int cos_v177_self_test(void) {
    cos_v177_state_t s;
    cos_v177_init(&s, 0x177CAFEBABEULL);
    cos_v177_build_fixture(&s);
    cos_v177_prune(&s);
    cos_v177_mixed_precision(&s);
    cos_v177_merge_layers(&s);
    cos_v177_report(&s);

    const cos_v177_report_t *r = &s.report;

    /* 1. pruning: did work (some neurons removed) */
    if (r->n_neurons_pruned <= 0) return 1;
    if (r->params_after >= r->params_before) return 2;

    /* 2. bytes after ≤ bytes before */
    if (r->bytes_after > r->bytes_before) return 3;

    /* 3. drift inside budget */
    if (r->sigma_drift_pct > s.drift_budget_pct) return 4;

    /* 4. mixed precision is diverse: at least INT8 and INT4 both used */
    if (r->n_layers_int8 <= 0) return 5;
    if (r->n_layers_int4 + r->n_layers_int2 <= 0) return 6;

    /* 5. at least one layer merged */
    if (r->n_layers_merged <= 0) return 7;

    /* 6. n_layers_after + merged == before */
    if (r->n_layers_after + r->n_layers_merged != r->n_layers_before) return 8;

    /* 7. params drop is material (≥ 30 %) */
    double ratio = (double)r->params_after / (double)r->params_before;
    if (ratio > 0.70) return 9;

    /* 8. Determinism */
    cos_v177_state_t a, b;
    cos_v177_init(&a, 0x177CAFEBABEULL);
    cos_v177_init(&b, 0x177CAFEBABEULL);
    cos_v177_build_fixture(&a);    cos_v177_build_fixture(&b);
    cos_v177_prune(&a);            cos_v177_prune(&b);
    cos_v177_mixed_precision(&a);  cos_v177_mixed_precision(&b);
    cos_v177_merge_layers(&a);     cos_v177_merge_layers(&b);
    cos_v177_report(&a);           cos_v177_report(&b);
    char A[32768], B[32768];
    size_t na = cos_v177_to_json(&a, A, sizeof(A));
    size_t nb = cos_v177_to_json(&b, B, sizeof(B));
    if (na == 0 || na != nb) return 10;
    if (memcmp(A, B, na) != 0) return 11;
    return 0;
}
