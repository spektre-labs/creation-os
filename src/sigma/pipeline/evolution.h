/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/*
 * σ-Evolution — evolutionary search over PIPELINE parameters,
 * not model weights.
 *
 * Nature MI (2025) "evolutionary merge recipes" showed you can
 * discover good LLM merges with a small genetic loop.  Creation OS
 * inherits the shape but points it at a different target: the
 * gates / thresholds / learning-rates that make σ-pipeline
 * primitives cheap and safe.  Genome:
 *
 *     τ_accept       σ-Agent accept threshold
 *     τ_rethink      σ-Reinforce rethink threshold
 *     ttt_lr         σ-TTT learning rate
 *     max_rethink    rethink cap
 *     codex_weight   Codex steering strength
 *     engram_ttl     σ-Engram entry TTL
 *
 * Fitness (caller-supplied benchmark callback) returns
 *
 *     accuracy, cost_eur, latency_s, sigma_bench
 *
 * and σ-Evolution computes
 *
 *     fitness = accuracy² · (1 / (cost_eur + ε)) · (1 / (latency_s + ε))
 *
 * where the squared accuracy keeps the "get the answer right"
 * axis dominant over "make it cheap".
 *
 * σ-guard: a mutant is ACCEPTED iff its sigma_bench is ≤ the best
 * sigma_bench of the previous generation plus a small slack.  We
 * walk strictly uphill in fitness and strictly downhill in σ.
 *
 * Contracts (v0):
 *   1. genome clamping is enforced on every mutation
 *      (τ ∈ [0,1], ttt_lr ∈ [0, 0.5], rethink ∈ [0, 16],
 *      codex_weight ∈ [0, 2], engram_ttl ∈ [0, 1e4]).
 *   2. step() runs: evaluate → sort → keep top half →
 *      crossover to fill + mutate.  All RNG runs off a seeded LCG
 *      so smoke tests pin the trajectory.
 *   3. Fitness ordering is stable (ties broken by genome index).
 *   4. Self-test runs a 4-member population, 3 generations, with
 *      a deterministic benchmark callback.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef COS_SIGMA_EVOLUTION_H
#define COS_SIGMA_EVOLUTION_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float tau_accept;
    float tau_rethink;
    float ttt_lr;
    int   max_rethink;
    float codex_weight;
    float engram_ttl;
} cos_evo_genome_t;

typedef struct {
    float accuracy;
    float cost_eur;
    float latency_s;
    float sigma_bench;
} cos_evo_benchmark_t;

typedef float (*cos_evo_benchmark_fn)(const cos_evo_genome_t *g,
                                      void *ctx,
                                      cos_evo_benchmark_t *out);

typedef struct {
    cos_evo_genome_t     genome;
    cos_evo_benchmark_t  bench;
    float                fitness;
    int                  born_generation;
} cos_evo_individual_t;

typedef struct {
    cos_evo_individual_t *pop;
    int                   pop_size;
    int                   generation;
    uint64_t              rng_state;
    float                 sigma_ceiling;   /* gen-best σ + slack     */
    float                 sigma_slack;
    float                 best_fitness;
    float                 best_sigma_bench;
    cos_evo_genome_t      best_genome;
} cos_evo_state_t;

void  cos_sigma_evo_genome_clamp(cos_evo_genome_t *g);

float cos_sigma_evo_fitness(const cos_evo_benchmark_t *b);

/* Seed a population with N copies of `seed_genome` plus per-gene
 * jitter from the LCG (caller-seeded). */
int   cos_sigma_evo_init(cos_evo_state_t *st,
                         cos_evo_individual_t *storage, int pop_size,
                         const cos_evo_genome_t *seed_genome,
                         uint64_t rng_seed,
                         float sigma_slack);

/* Run one generation: evaluate every individual, sort by fitness,
 * keep the top half, crossover+mutate the remainder, advance gen. */
int   cos_sigma_evo_step(cos_evo_state_t *st,
                         cos_evo_benchmark_fn benchmark,
                         void *bench_ctx);

/* Run `n_generations` steps and stamp the best genome in `out`. */
int   cos_sigma_evo_run(cos_evo_state_t *st, int n_generations,
                        cos_evo_benchmark_fn benchmark,
                        void *bench_ctx,
                        cos_evo_genome_t *out_best);

int   cos_sigma_evo_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_SIGMA_EVOLUTION_H */
