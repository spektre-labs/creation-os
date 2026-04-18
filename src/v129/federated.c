/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (c) 2026 Spektre Labs / Lauri Rainio
 *
 * v129 σ-Federated — implementation (pure C, deterministic).
 */
#include "federated.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void cos_v129_config_defaults(cos_v129_config_t *cfg) {
    if (!cfg) return;
    cfg->alpha_dp   = COS_V129_ALPHA_DP_DEFAULT;
    cfg->base_noise = COS_V129_BASE_NOISE_DEFAULT;
    cfg->k_base     = COS_V129_K_BASE_DEFAULT;
    cfg->k_min      = COS_V129_K_MIN_DEFAULT;
    cfg->k_max      = COS_V129_K_MAX_DEFAULT;
}

/* ====================================================================
 * σ-weighted FedAvg
 * ================================================================== */

int cos_v129_compute_weights(const cos_v129_node_delta_t *nodes,
                             int n, float *out_w) {
    if (!nodes || !out_w || n <= 0) return -1;
    double s = 0.0;
    for (int i = 0; i < n; ++i) {
        double sig = nodes[i].sigma_node;
        if (sig < 0.0) sig = 0.0;
        double w = 1.0 / ((double)COS_V129_EPS_SIGMA + sig);
        out_w[i] = (float)w;
        s += w;
    }
    if (s <= 0.0) return -1;
    for (int i = 0; i < n; ++i) out_w[i] = (float)(out_w[i] / s);
    return 0;
}

int cos_v129_aggregate(const cos_v129_node_delta_t *nodes, int n,
                       float *out_delta, int dim,
                       cos_v129_aggregate_stats_t *stats) {
    if (!nodes || !out_delta || n <= 0 || dim <= 0) return -1;
    for (int i = 0; i < n; ++i)
        if (!nodes[i].delta || nodes[i].dim != dim) return -1;

    float *w = (float*)malloc(sizeof(float) * (size_t)n);
    if (!w) return -1;
    if (cos_v129_compute_weights(nodes, n, w) < 0) { free(w); return -1; }

    for (int k = 0; k < dim; ++k) out_delta[k] = 0.0f;
    for (int i = 0; i < n; ++i) {
        for (int k = 0; k < dim; ++k)
            out_delta[k] += w[i] * nodes[i].delta[k];
    }

    if (stats) {
        memset(stats, 0, sizeof *stats);
        stats->n_nodes     = n;
        stats->dim         = dim;
        stats->weight_min  = w[0];
        stats->weight_max  = w[0];
        stats->winner_node_id = nodes[0].node_id;
        float wsum = 0.0f;
        for (int i = 0; i < n; ++i) {
            wsum += w[i];
            if (w[i] < stats->weight_min) stats->weight_min = w[i];
            if (w[i] > stats->weight_max) {
                stats->weight_max     = w[i];
                stats->winner_node_id = nodes[i].node_id;
            }
        }
        stats->weight_sum = wsum;
        double s = 0.0;
        for (int k = 0; k < dim; ++k) s += fabs(out_delta[k]);
        stats->mean_abs_delta = (float)(s / dim);
    }
    free(w);
    return 0;
}

/* ====================================================================
 * Deterministic Gaussian via SplitMix64 + Box-Muller
 * ================================================================== */

uint64_t cos_v129_rng_splitmix64(uint64_t *state) {
    uint64_t x = (*state += 0x9E3779B97F4A7C15ULL);
    x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ULL;
    x = (x ^ (x >> 27)) * 0x94D049BB133111EBULL;
    return x ^ (x >> 31);
}

static double rng_uniform(uint64_t *state) {
    /* [0,1) with 53 bits. */
    uint64_t x = cos_v129_rng_splitmix64(state);
    return (double)(x >> 11) * (1.0 / 9007199254740992.0);
}

float cos_v129_rng_gauss(uint64_t *state) {
    /* Box-Muller: guard against u1=0. */
    double u1, u2;
    do { u1 = rng_uniform(state); } while (u1 < 1e-300);
    u2 = rng_uniform(state);
    double r     = sqrt(-2.0 * log(u1));
    double theta = 2.0 * 3.141592653589793 * u2;
    return (float)(r * cos(theta));
}

float cos_v129_add_dp_noise(float *delta, int dim,
                            float sigma_node, float epsilon_budget,
                            const cos_v129_config_t *cfg,
                            uint64_t *rng_state) {
    if (!delta || !cfg || !rng_state || dim <= 0) return 0.0f;
    float eps = epsilon_budget;
    if (eps < 0.01f) eps = 0.01f;
    float sig_dp = cfg->base_noise
                 * (1.0f + cfg->alpha_dp * (sigma_node < 0 ? 0 : sigma_node))
                 / eps;
    for (int i = 0; i < dim; ++i)
        delta[i] += sig_dp * cos_v129_rng_gauss(rng_state);
    return sig_dp;
}

/* ====================================================================
 * σ-adaptive top-K sparsification
 * ================================================================== */

int cos_v129_adaptive_k(const cos_v129_config_t *cfg, float sigma_node) {
    if (!cfg) return 0;
    float s = sigma_node < 0 ? 0 : (sigma_node > 1 ? 1 : sigma_node);
    int k = (int)((float)cfg->k_base * (1.0f - s) + 0.5f);
    if (k < cfg->k_min) k = cfg->k_min;
    if (k > cfg->k_max) k = cfg->k_max;
    return k;
}

/* O(dim · k) selection — fine for LoRA deltas (K ≤ 256). */
int cos_v129_sparsify_topk(const float *delta, int dim, int k,
                           int *out_idx, float *out_val) {
    if (!delta || !out_idx || dim <= 0 || k <= 0) return 0;
    if (k > dim) k = dim;
    char *used = (char*)calloc((size_t)dim, sizeof(char));
    if (!used) return 0;
    int filled = 0;
    for (int j = 0; j < k; ++j) {
        int   best_i = -1;
        float best_v = -1.0f;
        for (int i = 0; i < dim; ++i) {
            if (used[i]) continue;
            float a = fabsf(delta[i]);
            if (a > best_v) { best_v = a; best_i = i; }
        }
        if (best_i < 0) break;
        used[best_i]   = 1;
        out_idx[filled] = best_i;
        if (out_val) out_val[filled] = delta[best_i];
        filled++;
    }
    free(used);
    return filled;
}

/* ====================================================================
 * Unlearning
 * ================================================================== */

int cos_v129_unlearn_diff(const float *contribution, int dim,
                          float weight, float *out_diff) {
    if (!contribution || !out_diff || dim <= 0) return -1;
    for (int i = 0; i < dim; ++i)
        out_diff[i] = -weight * contribution[i];
    return 0;
}

int cos_v129_stats_to_json(const cos_v129_aggregate_stats_t *s,
                           char *out, size_t cap) {
    if (!s || !out || cap == 0) return -1;
    return snprintf(out, cap,
        "{\"n_nodes\":%d,\"dim\":%d,"
        "\"weight_min\":%.6f,\"weight_max\":%.6f,"
        "\"weight_sum\":%.6f,\"winner_node_id\":%d,"
        "\"mean_abs_delta\":%.6f}",
        s->n_nodes, s->dim,
        (double)s->weight_min, (double)s->weight_max,
        (double)s->weight_sum, s->winner_node_id,
        (double)s->mean_abs_delta);
}

/* ====================================================================
 * Self-test
 * ================================================================== */

#define _CHECK(cond, msg) do { \
    if (!(cond)) { \
        fprintf(stderr, "v129 self-test FAIL: %s (line %d)\n", (msg), __LINE__); \
        return 1; \
    } \
} while (0)

int cos_v129_self_test(void) {
    cos_v129_config_t cfg;
    cos_v129_config_defaults(&cfg);

    /* --- σ-weighted aggregation picks low-σ node ---------------- */
    fprintf(stderr, "check-v129: σ-weighted FedAvg\n");
    enum { D = 4 };
    float d0[D] = { 1.0f, 2.0f, 3.0f, 4.0f };
    float d1[D] = { 0.5f, 0.5f, 0.5f, 0.5f };
    float d2[D] = {-1.0f,-1.0f,-1.0f,-1.0f };
    cos_v129_node_delta_t nodes[3] = {
        { .node_id=10, .sigma_node=0.10f, .delta=d0, .dim=D },
        { .node_id=11, .sigma_node=0.50f, .delta=d1, .dim=D },
        { .node_id=12, .sigma_node=0.90f, .delta=d2, .dim=D },
    };
    float out[D]; cos_v129_aggregate_stats_t stats;
    int rc = cos_v129_aggregate(nodes, 3, out, D, &stats);
    _CHECK(rc == 0, "aggregate rc");
    _CHECK(stats.winner_node_id == 10, "low-σ node is winner");
    _CHECK(fabsf(stats.weight_sum - 1.0f) < 1e-4f, "weights sum to 1");
    /* node 10 weight ≈ 10/(10+2+1.111) ≈ 0.762 → dominates */
    _CHECK(stats.weight_max > 0.70f, "low-σ weight > 0.70");
    _CHECK(stats.weight_min < 0.12f, "high-σ weight < 0.12");
    /* Sanity: aggregate is pulled toward d0 */
    _CHECK(out[0] > 0.60f, "output pulled toward d0[0]=1.0");

    /* --- Uniform σ → classical FedAvg (equal weights) ------------- */
    fprintf(stderr, "check-v129: uniform σ → equal weights\n");
    nodes[0].sigma_node = nodes[1].sigma_node = nodes[2].sigma_node = 0.30f;
    rc = cos_v129_aggregate(nodes, 3, out, D, &stats);
    _CHECK(rc == 0, "uniform rc");
    _CHECK(fabsf(stats.weight_max - stats.weight_min) < 1e-4f,
                                                  "weights equal on uniform σ");
    /* out = (d0 + d1 + d2) / 3 ≈ (0.1667, 0.5, 0.8333, 1.1667) */
    _CHECK(fabsf(out[1] - 0.5f) < 1e-4f, "uniform aggregate component ok");

    /* --- Gauss RNG determinism + rough stats ---------------------- */
    fprintf(stderr, "check-v129: SplitMix Gaussian RNG determinism + moments\n");
    uint64_t st1 = 42, st2 = 42;
    float s1 = cos_v129_rng_gauss(&st1);
    float s2 = cos_v129_rng_gauss(&st2);
    _CHECK(s1 == s2, "same seed → same sample");
    /* 4k samples: mean ≈ 0, stddev ≈ 1 (loose bounds) */
    uint64_t st = 1234567ULL;
    double sum = 0.0, sum2 = 0.0;
    for (int i = 0; i < 4096; ++i) {
        float g = cos_v129_rng_gauss(&st);
        sum  += g;
        sum2 += (double)g * (double)g;
    }
    double m = sum / 4096.0;
    double v = sum2 / 4096.0 - m * m;
    _CHECK(fabs(m)       < 0.1,  "gauss mean ≈ 0");
    _CHECK(fabs(v - 1.0) < 0.1,  "gauss var ≈ 1");

    /* --- DP noise: higher σ_node → higher σ_DP -------------------- */
    fprintf(stderr, "check-v129: σ-scaled DP noise\n");
    float dp_low [16] = {0};
    float dp_high[16] = {0};
    uint64_t rs = 0xC0DEC0DE;
    float sig_dp_low  = cos_v129_add_dp_noise(dp_low , 16, 0.10f, 1.0f,
                                              &cfg, &rs);
    float sig_dp_high = cos_v129_add_dp_noise(dp_high, 16, 0.90f, 1.0f,
                                              &cfg, &rs);
    _CHECK(sig_dp_high > sig_dp_low, "high-σ node → more DP noise");

    /* --- σ-adaptive K --------------------------------------------- */
    fprintf(stderr, "check-v129: σ-adaptive top-K\n");
    int k_confident = cos_v129_adaptive_k(&cfg, 0.10f);
    int k_uncertain = cos_v129_adaptive_k(&cfg, 0.90f);
    _CHECK(k_confident > k_uncertain, "confident → more coords shared");
    _CHECK(k_confident >= 32 && k_confident <= cfg.k_max,
                                                  "k_confident in band");
    _CHECK(k_uncertain <= 16 && k_uncertain >= cfg.k_min,
                                                  "k_uncertain in band");

    /* --- Sparsify picks largest |values| -------------------------- */
    fprintf(stderr, "check-v129: sparsify picks largest magnitudes\n");
    float dd[8] = { 0.1f, -0.9f, 0.2f, -0.3f, 0.8f, 0.05f, -0.7f, 0.4f };
    int idx[3]; float val[3];
    int nk = cos_v129_sparsify_topk(dd, 8, 3, idx, val);
    _CHECK(nk == 3, "got 3");
    _CHECK(idx[0] == 1, "biggest |−0.9|");
    _CHECK(idx[1] == 4, "next |0.8|");
    _CHECK(idx[2] == 6, "next |−0.7|");

    /* --- Unlearn round-trip --------------------------------------- */
    fprintf(stderr, "check-v129: unlearn diff round-trip\n");
    /* Aggregate 2 nodes, then remove node 12 via unlearn_diff. */
    cos_v129_node_delta_t two[2] = {
        { .node_id=1, .sigma_node=0.20f, .delta=d0, .dim=D },
        { .node_id=2, .sigma_node=0.20f, .delta=d2, .dim=D },
    };
    float agg[D], w[2]; float without2_expected[D];
    cos_v129_aggregate(two, 2, agg, D, NULL);
    cos_v129_compute_weights(two, 2, w);
    /* Reference: aggregate of just {d0} under σ=0.20 alone is d0. */
    for (int i = 0; i < D; ++i) without2_expected[i] = d0[i];
    float diff[D];
    cos_v129_unlearn_diff(d2, D, w[1], diff);
    /* Apply diff; also re-scale the remaining-node contribution back to
     * unit weight (removing a node changes the weight sum).  For the
     * unit-weight residual test, both nodes had equal weights 0.5, so
     * after subtraction out/0.5 ≡ d0 component. */
    for (int i = 0; i < D; ++i) agg[i] += diff[i];
    /* agg now equals 0.5·d0 (the contribution of d0 at its old weight).
     * Divide by its weight to recover d0 itself. */
    for (int i = 0; i < D; ++i) {
        float recovered = agg[i] / w[0];
        _CHECK(fabsf(recovered - without2_expected[i]) < 1e-4f,
               "unlearn recovers d0");
    }

    /* --- JSON shape ----------------------------------------------- */
    fprintf(stderr, "check-v129: JSON shape\n");
    char js[256];
    int wsz = cos_v129_stats_to_json(&stats, js, sizeof js);
    _CHECK(wsz > 0 && wsz < (int)sizeof js, "stats JSON wrote");
    _CHECK(strstr(js, "\"n_nodes\":3") != NULL, "stats shape");

    fprintf(stderr, "check-v129: OK (σ-FedAvg + DP noise + top-K + unlearn)\n");
    return 0;
}
