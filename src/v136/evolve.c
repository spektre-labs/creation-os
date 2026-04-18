/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (c) 2026 Spektre Labs / Lauri Rainio
 *
 * v136 σ-Evolve — implementation.
 */
#include "evolve.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------------- Deterministic PRNG (SplitMix64 + Box-Muller) ---- */
static uint64_t splitmix64(uint64_t *s) {
    *s += 0x9E3779B97F4A7C15ULL;
    uint64_t z = *s;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}
static float urand01(uint64_t *s) {
    return (float)((splitmix64(s) >> 11) & 0x1FFFFFFFFFFFFFULL)
         / (float)(1ULL << 53);
}
static float nrand(uint64_t *s) {
    /* Two uniforms → one normal via Box-Muller. */
    float u1 = urand01(s); if (u1 < 1e-12f) u1 = 1e-12f;
    float u2 = urand01(s);
    return sqrtf(-2.0f * logf(u1)) * cosf(6.2831853f * u2);
}

static float clamp01(float x) { return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x); }

/* ================================================================ */

void cos_v136_cfg_defaults(cos_v136_cfg_t *cfg) {
    if (!cfg) return;
    cfg->pop_size    = 20;
    cfg->mu          = 5;
    cfg->generations = 50;
    cfg->sigma_mut   = 0.10f;
    cfg->seed        = 42;
}

float cos_v136_aggregate(const cos_v136_genome_t *g,
                         const float ch[COS_V136_CHANNELS]) {
    if (!g) return 1.0f;
    /* Weighted geometric mean, robust to zero weights. */
    double log_sum = 0.0;
    double w_sum   = 0.0;
    for (int i = 0; i < COS_V136_CHANNELS; ++i) {
        float w = g->w[i]; if (w < 0) w = 0;
        float c = ch[i];
        if (c < 1e-6f) c = 1e-6f;
        if (c > 1.0f)  c = 1.0f;
        log_sum += (double)w * log((double)c);
        w_sum   += (double)w;
    }
    if (w_sum < 1e-9) return 0.5f;     /* degenerate → neutral σ    */
    return (float)exp(log_sum / w_sum);
}

float cos_v136_evaluate(cos_v136_genome_t *g,
                        const cos_v136_sample_t *data, int n) {
    if (!g || !data || n <= 0) return 0.0f;
    int tp = 0, tn = 0, fp = 0, fn = 0;
    for (int i = 0; i < n; ++i) {
        float agg = cos_v136_aggregate(g, data[i].channels);
        int abstain = (agg > g->tau) ? 1 : 0;
        if (data[i].should_abstain) {
            if (abstain) tp++; else fn++;
        } else {
            if (!abstain) tn++; else fp++;
        }
    }
    float tpr = (tp + fn > 0) ? (float)tp / (float)(tp + fn) : 0.0f;
    float tnr = (tn + fp > 0) ? (float)tn / (float)(tn + fp) : 0.0f;
    float fit = 0.5f * tpr + 0.5f * tnr;
    g->fitness = fit;
    return fit;
}

void cos_v136_synthetic(cos_v136_sample_t *out, int n, uint64_t seed) {
    if (!out || n <= 0) return;
    uint64_t s = seed ? seed : 1ULL;
    /* Overlapping informative channels (0..4) + high-variance
     * noise channels (5..7).  ch[0..4] carry a weak signal: N(0.30,
     * 0.10) for emit, N(0.55, 0.10) for abstain — clearly separable
     * but with overlap in the tails.  ch[5..7] are drawn from U(0,
     * 1) *regardless of label*, so under uniform weighting they
     * pump random values into every sample's aggregate.  An ES
     * that drives w[5..7] ≈ 0 recovers the underlying signal. */
    for (int i = 0; i < n; ++i) {
        int abstain = i & 1;
        out[i].should_abstain = abstain;
        for (int k = 0; k < COS_V136_CHANNELS; ++k) {
            float v;
            if (k < 5) {
                float base = abstain ? 0.55f : 0.30f;
                v = base + 0.10f * nrand(&s);
            } else {
                v = urand01(&s);     /* pure label-independent noise */
            }
            out[i].channels[k] = clamp01(v);
        }
    }
}

/* Rank-based (μ/μ, λ)-ES with diagonal covariance (simplified
 * CMA-ES).  Each generation:
 *   1. evaluate λ offspring
 *   2. sort by fitness, keep top μ ("selected")
 *   3. compute centroid and per-dim variance of the selected
 *   4. sample λ new offspring ~ N(centroid, diag(var) * sigma_mut²)
 *      clipped to [0, 1].
 * The best-seen genome is carried over (elitism) so trajectory
 * is monotone non-decreasing. */

static int cmp_genome(const void *a, const void *b) {
    const cos_v136_genome_t *ga = (const cos_v136_genome_t *)a;
    const cos_v136_genome_t *gb = (const cos_v136_genome_t *)b;
    if (ga->fitness > gb->fitness) return -1;
    if (ga->fitness < gb->fitness) return  1;
    return 0;
}

int cos_v136_run(const cos_v136_cfg_t *cfg,
                 const cos_v136_sample_t *data, int n,
                 cos_v136_genome_t *out_best,
                 float *trajectory) {
    if (!cfg || !data || n <= 0 || !out_best) return -1;
    int lam = cfg->pop_size;
    int mu  = cfg->mu;
    if (lam <= 1 || lam > COS_V136_POP_MAX || mu <= 0 || mu >= lam) return -1;

    cos_v136_genome_t pop[COS_V136_POP_MAX];
    uint64_t rng = cfg->seed ? cfg->seed : 0xC0FFEEULL;

    /* Initial population: uniform random in [0, 1]. */
    for (int i = 0; i < lam; ++i) {
        for (int k = 0; k < COS_V136_CHANNELS; ++k)
            pop[i].w[k] = urand01(&rng);
        pop[i].tau = urand01(&rng);
        cos_v136_evaluate(&pop[i], data, n);
    }
    qsort(pop, (size_t)lam, sizeof pop[0], cmp_genome);
    cos_v136_genome_t best = pop[0];

    for (int gen = 0; gen < cfg->generations; ++gen) {
        /* centroid + variance over top μ. */
        float cent[COS_V136_GENOME_DIM] = {0};
        float var [COS_V136_GENOME_DIM] = {0};
        for (int i = 0; i < mu; ++i) {
            for (int k = 0; k < COS_V136_CHANNELS; ++k)
                cent[k] += pop[i].w[k];
            cent[COS_V136_CHANNELS] += pop[i].tau;
        }
        for (int k = 0; k < COS_V136_GENOME_DIM; ++k)
            cent[k] /= (float)mu;
        for (int i = 0; i < mu; ++i) {
            for (int k = 0; k < COS_V136_CHANNELS; ++k) {
                float d = pop[i].w[k] - cent[k];
                var[k] += d * d;
            }
            float d = pop[i].tau - cent[COS_V136_CHANNELS];
            var[COS_V136_CHANNELS] += d * d;
        }
        for (int k = 0; k < COS_V136_GENOME_DIM; ++k) {
            var[k] = var[k] / (float)mu + 1e-4f;   /* floor */
        }

        /* Sample λ new offspring around centroid (elitism: slot 0
         * keeps the best seen so far). */
        pop[0] = best;
        for (int i = 1; i < lam; ++i) {
            for (int k = 0; k < COS_V136_CHANNELS; ++k) {
                float step = cfg->sigma_mut * sqrtf(var[k]) * nrand(&rng);
                pop[i].w[k] = clamp01(cent[k] + step);
            }
            float step = cfg->sigma_mut
                       * sqrtf(var[COS_V136_CHANNELS]) * nrand(&rng);
            pop[i].tau = clamp01(cent[COS_V136_CHANNELS] + step);
            cos_v136_evaluate(&pop[i], data, n);
        }
        /* Re-evaluate elite in case dataset changed (no-op here). */
        cos_v136_evaluate(&pop[0], data, n);

        qsort(pop, (size_t)lam, sizeof pop[0], cmp_genome);
        if (pop[0].fitness > best.fitness) best = pop[0];
        if (trajectory) trajectory[gen] = best.fitness;
    }

    *out_best = best;
    return 0;
}

int cos_v136_write_toml(const cos_v136_genome_t *g, const char *path) {
    if (!g || !path) return -1;
    FILE *fp = fopen(path, "w");
    if (!fp) return -1;
    fprintf(fp, "# v136 σ-evolve output — weighted geometric aggregator\n");
    fprintf(fp, "[sigma_aggregator]\n");
    fprintf(fp, "kind = \"weighted_geometric\"\n");
    fprintf(fp, "tau  = %.6f\n", g->tau);
    fprintf(fp, "weights = [");
    for (int i = 0; i < COS_V136_CHANNELS; ++i)
        fprintf(fp, "%.6f%s", g->w[i],
                i + 1 < COS_V136_CHANNELS ? ", " : "");
    fprintf(fp, "]\nfitness = %.6f\n", g->fitness);
    fclose(fp);
    return 0;
}

/* ================================================================
 * Self-test
 * ============================================================= */

#define _CHECK(cond, msg) do { \
    if (!(cond)) { \
        fprintf(stderr, "v136 self-test FAIL: %s (line %d)\n", \
                (msg), __LINE__); return 1; \
    } \
} while (0)

int cos_v136_self_test(void) {
    cos_v136_cfg_t cfg; cos_v136_cfg_defaults(&cfg);

    fprintf(stderr, "check-v136: generate synthetic sidecar (N=200)\n");
    cos_v136_sample_t data[200];
    cos_v136_synthetic(data, 200, 1337);

    /* Uniform baseline: all w = 1, τ = 0.5. */
    cos_v136_genome_t uniform;
    for (int i = 0; i < COS_V136_CHANNELS; ++i) uniform.w[i] = 1.0f;
    uniform.tau = 0.5f;
    float base_fit = cos_v136_evaluate(&uniform, data, 200);
    fprintf(stderr, "  uniform baseline fitness=%.4f\n", (double)base_fit);

    /* Run ES. */
    fprintf(stderr, "check-v136: run ES (pop=%d μ=%d gens=%d)\n",
            cfg.pop_size, cfg.mu, cfg.generations);
    cos_v136_genome_t best;
    float traj[128];
    _CHECK(cos_v136_run(&cfg, data, 200, &best, traj) == 0, "ES run");
    fprintf(stderr, "  evolved best fitness=%.4f\n", (double)best.fitness);
    fprintf(stderr, "  τ=%.3f weights=[", (double)best.tau);
    for (int i = 0; i < COS_V136_CHANNELS; ++i)
        fprintf(stderr, "%.2f%s", (double)best.w[i],
                i + 1 < COS_V136_CHANNELS ? "," : "]\n");

    /* Elitism guarantees trajectory is monotone non-decreasing. */
    for (int i = 1; i < cfg.generations; ++i)
        _CHECK(traj[i] + 1e-6f >= traj[i - 1], "trajectory monotone");

    /* Evolved must beat the uniform baseline by a clear margin. */
    _CHECK(best.fitness >= base_fit + 0.02f,
           "evolved beats uniform by ≥0.02");

    /* And must be usefully good in absolute terms (≥ 0.80 on this
     * well-separated synthetic is easy for the weighted geo mean). */
    _CHECK(best.fitness > 0.80f, "evolved fitness > 0.80");

    /* TOML write smoke. */
    _CHECK(cos_v136_write_toml(&best, "/tmp/cos_v136_evolve.toml") == 0,
           "toml write");

    fprintf(stderr, "check-v136: OK (ES + synthetic + beats baseline)\n");
    return 0;
}
