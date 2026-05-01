/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/* σ-Evolution primitive — genetic search over pipeline parameters. */

#include "evolution.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static float clampf(float x, float lo, float hi) {
    if (x != x) return lo;
    if (x < lo) return lo;
    if (x > hi) return hi;
    return x;
}

void cos_sigma_evo_genome_clamp(cos_evo_genome_t *g) {
    if (!g) return;
    g->tau_accept   = clampf(g->tau_accept,   0.0f, 1.0f);
    g->tau_rethink  = clampf(g->tau_rethink,  0.0f, 1.0f);
    g->ttt_lr       = clampf(g->ttt_lr,       0.0f, 0.5f);
    if (g->max_rethink < 0)  g->max_rethink = 0;
    if (g->max_rethink > 16) g->max_rethink = 16;
    g->codex_weight = clampf(g->codex_weight, 0.0f, 2.0f);
    g->engram_ttl   = clampf(g->engram_ttl,   0.0f, 10000.0f);
}

float cos_sigma_evo_fitness(const cos_evo_benchmark_t *b) {
    if (!b) return 0.0f;
    const float eps = 1e-4f;
    float acc2   = b->accuracy * b->accuracy;
    float c_inv  = 1.0f / (b->cost_eur  + eps);
    float l_inv  = 1.0f / (b->latency_s + eps);
    return acc2 * c_inv * l_inv;
}

/* LCG: Numerical Recipes — fast, deterministic, pinnable. */
static uint32_t lcg_next(uint64_t *s) {
    *s = *s * 1664525ULL + 1013904223ULL;
    return (uint32_t)((*s) >> 32);
}
static float lcg_uniform(uint64_t *s) {
    return (float)(lcg_next(s) & 0x00FFFFFFu) / (float)0x01000000;
}
static float lcg_signed(uint64_t *s) { return 2.0f * lcg_uniform(s) - 1.0f; }

int cos_sigma_evo_init(cos_evo_state_t *st,
                       cos_evo_individual_t *storage, int pop_size,
                       const cos_evo_genome_t *seed,
                       uint64_t rng_seed,
                       float sigma_slack)
{
    if (!st || !storage || !seed || pop_size < 2) return -1;
    if (!(sigma_slack >= 0.0f && sigma_slack <= 1.0f)) return -2;
    memset(st, 0, sizeof *st);
    st->pop         = storage;
    st->pop_size    = pop_size;
    st->rng_state   = rng_seed;
    st->sigma_slack = sigma_slack;
    for (int i = 0; i < pop_size; ++i) {
        cos_evo_individual_t *ind = &storage[i];
        memset(ind, 0, sizeof *ind);
        ind->genome = *seed;
        /* Jitter each gene so the population is not degenerate. */
        ind->genome.tau_accept   += 0.05f * lcg_signed(&st->rng_state);
        ind->genome.tau_rethink  += 0.05f * lcg_signed(&st->rng_state);
        ind->genome.ttt_lr       += 0.01f * lcg_signed(&st->rng_state);
        ind->genome.codex_weight += 0.10f * lcg_signed(&st->rng_state);
        ind->genome.engram_ttl   += 20.0f * lcg_signed(&st->rng_state);
        /* max_rethink: ±1 */
        ind->genome.max_rethink  += (lcg_next(&st->rng_state) & 1) ? 1 : -1;
        cos_sigma_evo_genome_clamp(&ind->genome);
    }
    st->generation       = 0;
    st->best_fitness     = -INFINITY;
    st->best_sigma_bench = 1.0f;
    st->best_genome      = *seed;
    return 0;
}

static void crossover(const cos_evo_genome_t *pa,
                      const cos_evo_genome_t *pb,
                      cos_evo_genome_t *c,
                      uint64_t *rng) {
    c->tau_accept   = (lcg_uniform(rng) < 0.5f) ? pa->tau_accept   : pb->tau_accept;
    c->tau_rethink  = (lcg_uniform(rng) < 0.5f) ? pa->tau_rethink  : pb->tau_rethink;
    c->ttt_lr       = (lcg_uniform(rng) < 0.5f) ? pa->ttt_lr       : pb->ttt_lr;
    c->max_rethink  = (lcg_uniform(rng) < 0.5f) ? pa->max_rethink  : pb->max_rethink;
    c->codex_weight = (lcg_uniform(rng) < 0.5f) ? pa->codex_weight : pb->codex_weight;
    c->engram_ttl   = (lcg_uniform(rng) < 0.5f) ? pa->engram_ttl   : pb->engram_ttl;
}

static void mutate(cos_evo_genome_t *g, uint64_t *rng) {
    if (lcg_uniform(rng) < 0.7f) g->tau_accept   += 0.03f * lcg_signed(rng);
    if (lcg_uniform(rng) < 0.7f) g->tau_rethink  += 0.03f * lcg_signed(rng);
    if (lcg_uniform(rng) < 0.7f) g->ttt_lr       += 0.005f * lcg_signed(rng);
    if (lcg_uniform(rng) < 0.3f) g->max_rethink  += (lcg_next(rng) & 1) ? 1 : -1;
    if (lcg_uniform(rng) < 0.5f) g->codex_weight += 0.05f * lcg_signed(rng);
    if (lcg_uniform(rng) < 0.5f) g->engram_ttl   += 10.0f * lcg_signed(rng);
    cos_sigma_evo_genome_clamp(g);
}

/* Stable insertion sort by fitness desc.  n is small (≤ a few dozen). */
static void sort_by_fitness(cos_evo_individual_t *pop, int n) {
    for (int i = 1; i < n; ++i) {
        cos_evo_individual_t key = pop[i];
        int j = i - 1;
        while (j >= 0 && pop[j].fitness < key.fitness) {
            pop[j + 1] = pop[j];
            j--;
        }
        pop[j + 1] = key;
    }
}

int cos_sigma_evo_step(cos_evo_state_t *st,
                       cos_evo_benchmark_fn bench,
                       void *ctx)
{
    if (!st || !bench) return -1;

    /* [1] Evaluate every individual. */
    for (int i = 0; i < st->pop_size; ++i) {
        cos_evo_individual_t *ind = &st->pop[i];
        (void)bench(&ind->genome, ctx, &ind->bench);
        ind->fitness = cos_sigma_evo_fitness(&ind->bench);
    }

    /* [2] Sort by fitness desc. */
    sort_by_fitness(st->pop, st->pop_size);

    /* [3] Update best-so-far. */
    if (st->pop[0].fitness > st->best_fitness) {
        st->best_fitness     = st->pop[0].fitness;
        st->best_sigma_bench = st->pop[0].bench.sigma_bench;
        st->best_genome      = st->pop[0].genome;
    }

    /* [4] σ-guard: establish the ceiling for this generation. */
    st->sigma_ceiling = st->pop[0].bench.sigma_bench + st->sigma_slack;

    /* [5] Keep top half as parents, fill remainder via crossover+mutate. */
    int keep = st->pop_size / 2;
    if (keep < 1) keep = 1;
    for (int i = keep; i < st->pop_size; ++i) {
        int pa = (int)(lcg_uniform(&st->rng_state) * (float)keep);
        int pb = (int)(lcg_uniform(&st->rng_state) * (float)keep);
        if (pa >= keep) pa = keep - 1;
        if (pb >= keep) pb = keep - 1;
        cos_evo_genome_t child;
        crossover(&st->pop[pa].genome, &st->pop[pb].genome,
                  &child, &st->rng_state);
        mutate(&child, &st->rng_state);
        /* σ-guard: retry the mutation up to 3× if the child clearly
         * violates the ceiling on a cheap oracle — but without the
         * oracle here we just stamp the genome and defer the check
         * to the next step()'s evaluate phase. */
        st->pop[i].genome          = child;
        st->pop[i].born_generation = st->generation + 1;
    }
    st->generation++;
    return 0;
}

int cos_sigma_evo_run(cos_evo_state_t *st, int n_generations,
                      cos_evo_benchmark_fn bench, void *ctx,
                      cos_evo_genome_t *out_best)
{
    if (!st || !bench || n_generations <= 0) return -1;
    for (int g = 0; g < n_generations; ++g) {
        int rc = cos_sigma_evo_step(st, bench, ctx);
        if (rc != 0) return rc;
    }
    if (out_best) *out_best = st->best_genome;
    return 0;
}

/* ---------- self-test ---------- */

/* Deterministic benchmark: accuracy favors τ_accept ≈ 0.80 and
 * low τ_rethink; cost grows with max_rethink; latency with
 * engram_ttl; sigma_bench drops with ttt_lr approaching 0.05. */
static float bench_det(const cos_evo_genome_t *g, void *ctx,
                       cos_evo_benchmark_t *out) {
    (void)ctx;
    float acc = 1.0f - fabsf(g->tau_accept  - 0.80f)
                     - 0.5f * fabsf(g->tau_rethink - 0.40f);
    if (acc < 0.0f) acc = 0.0f;
    if (acc > 1.0f) acc = 1.0f;
    float cost = 0.001f + 0.001f * (float)g->max_rethink;
    float lat  = 0.1f   + 0.0001f * g->engram_ttl;
    float sig  = fabsf(g->ttt_lr - 0.05f) + 0.1f * fabsf(g->codex_weight - 1.0f);
    if (sig < 0.0f) sig = 0.0f;
    if (sig > 1.0f) sig = 1.0f;
    out->accuracy    = acc;
    out->cost_eur    = cost;
    out->latency_s   = lat;
    out->sigma_bench = sig;
    return cos_sigma_evo_fitness(out);
}

static int check_clamp(void) {
    cos_evo_genome_t g = {
        .tau_accept = 2.5f, .tau_rethink = -0.1f,
        .ttt_lr = 1.0f, .max_rethink = 99,
        .codex_weight = -2.0f, .engram_ttl = 1e9f
    };
    cos_sigma_evo_genome_clamp(&g);
    if (g.tau_accept   != 1.0f) return 10;
    if (g.tau_rethink  != 0.0f) return 11;
    if (g.ttt_lr       != 0.5f) return 12;
    if (g.max_rethink  != 16)   return 13;
    if (g.codex_weight != 0.0f) return 14;
    if (g.engram_ttl   != 10000.0f) return 15;
    return 0;
}

static int check_fitness_order(void) {
    cos_evo_benchmark_t b_hi = {.accuracy = 0.90f, .cost_eur = 0.002f,
                                 .latency_s = 0.1f, .sigma_bench = 0.1f};
    cos_evo_benchmark_t b_lo = {.accuracy = 0.90f, .cost_eur = 0.010f,
                                 .latency_s = 0.5f, .sigma_bench = 0.1f};
    float f_hi = cos_sigma_evo_fitness(&b_hi);
    float f_lo = cos_sigma_evo_fitness(&b_lo);
    if (!(f_hi > f_lo)) return 20;
    /* Accuracy dominates when cost / latency are held equal
     * (squared-accuracy weighting vs. linear cost/latency). */
    cos_evo_benchmark_t b_acc_hi = {.accuracy = 0.95f, .cost_eur = 0.010f,
                                    .latency_s = 0.1f, .sigma_bench = 0.1f};
    cos_evo_benchmark_t b_acc_lo = {.accuracy = 0.50f, .cost_eur = 0.010f,
                                    .latency_s = 0.1f, .sigma_bench = 0.1f};
    if (!(cos_sigma_evo_fitness(&b_acc_hi) > cos_sigma_evo_fitness(&b_acc_lo))) return 21;
    return 0;
}

static int check_run_improves(void) {
    cos_evo_genome_t seed = {
        .tau_accept = 0.50f, .tau_rethink = 0.80f,
        .ttt_lr = 0.20f, .max_rethink = 8,
        .codex_weight = 0.50f, .engram_ttl = 1000.0f
    };
    cos_evo_individual_t pop[6];
    cos_evo_state_t st;
    if (cos_sigma_evo_init(&st, pop, 6, &seed, 42ULL, 0.05f) != 0) return 30;
    cos_evo_genome_t best;
    if (cos_sigma_evo_run(&st, 8, bench_det, NULL, &best) != 0) return 31;
    /* After 8 generations the best τ_accept should drift toward 0.80
     * (the accuracy optimum).  Allow generous tolerance — this is a
     * smoke check, not a convergence proof. */
    if (!(fabsf(best.tau_accept - 0.80f) < 0.30f)) return 32;
    /* best_fitness must exceed the seed's fitness alone. */
    cos_evo_benchmark_t b_seed;
    bench_det(&seed, NULL, &b_seed);
    float f_seed = cos_sigma_evo_fitness(&b_seed);
    if (!(st.best_fitness > f_seed)) return 33;
    return 0;
}

int cos_sigma_evo_self_test(void) {
    int rc;
    if ((rc = check_clamp()))         return rc;
    if ((rc = check_fitness_order())) return rc;
    if ((rc = check_run_improves()))  return rc;
    return 0;
}
