/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/*
 * v163 σ-Evolve-Architecture — architecture evolution kernel.
 *
 * v136 σ-tune optimizes σ-aggregator parameters.  v163 evolves
 * the *architecture itself*: which kernels to enable, in what
 * order, with what configuration.  Each architecture is a
 * binary-vector genome over a small kernel set (12 genes),
 * with 3 deterministic fitness dimensions:
 *
 *     accuracy  = base + Σ gene_on × contribution − saturation
 *     latency   = Σ gene_on × latency_cost        (smaller is better)
 *     memory    = Σ gene_on × memory_cost         (smaller is better)
 *
 * A seeded CMA-ES-lite loop (population 12, 30 generations)
 * maintains a Bernoulli mean per gene and updates it toward the
 * elite quarter of the population at each step.  The final
 * population is reduced to a Pareto front (non-dominated
 * sorting on (accuracy ↑, latency ↓, memory ↓)) which we
 * require to contain ≥ 3 distinct non-dominated points.
 *
 * σ-gated evolution: every candidate's accuracy must stay
 * within 3 % of the baseline ("all genes on") v143 benchmark
 * score — candidates that regress more than that are rejected
 * by v133-meta during ranking, so the evolution cannot collapse
 * to a tiny-but-useless genome.
 *
 * Auto-profile generation: given a device budget (M3 8 GB,
 * default), we pick the Pareto point whose latency + memory
 * score fits the budget and emit its enabled-gene list — the
 * v162 profile the user actually wants.
 *
 * v163.0 (this file) is fully in-process: fitness is a closed-
 * form deterministic function over the genome (no real v143
 * inference).  v163.1 swaps in a real v143-smoke call per
 * candidate evaluation and writes the Pareto front back into
 * kernels/evolve/pareto.json.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V163_EVOLVE_H
#define COS_V163_EVOLVE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V163_N_GENES     12
#define COS_V163_POP         12
#define COS_V163_MAX_GEN     30
#define COS_V163_MAX_PARETO  12
#define COS_V163_NAME_MAX    24

typedef struct {
    char  id[COS_V163_NAME_MAX];
    float contribution;     /* accuracy bonus if on */
    float latency_cost_ms;
    float memory_cost_mb;
} cos_v163_gene_spec_t;

typedef struct {
    bool  genes[COS_V163_N_GENES];
    float accuracy;         /* v143 benchmark proxy */
    float latency_ms;
    float memory_mb;
    int   n_on;
    bool  sigma_gated;      /* true ⇒ rejected for regression */
} cos_v163_individual_t;

typedef struct {
    cos_v163_individual_t points[COS_V163_MAX_PARETO];
    int n_points;
} cos_v163_pareto_t;

typedef struct {
    uint64_t               seed;
    cos_v163_gene_spec_t   genes[COS_V163_N_GENES];
    cos_v163_individual_t  population[COS_V163_POP];
    int                    generation;
    float                  gene_means[COS_V163_N_GENES]; /* Bernoulli p's */
    float                  baseline_accuracy;
    float                  tau_regression;               /* e.g. 0.03 */
    cos_v163_individual_t  best_accuracy_so_far;
    cos_v163_pareto_t      pareto;
} cos_v163_search_t;

typedef struct {
    cos_v163_individual_t choice;
    float latency_budget_ms;
    float memory_budget_mb;
} cos_v163_auto_profile_t;

/* Initialize genes + baseline + Bernoulli means. */
void cos_v163_search_init(cos_v163_search_t *s, uint64_t seed);

/* Advance the search by one generation.  Returns the accuracy
 * of the best individual this generation. */
float cos_v163_search_step(cos_v163_search_t *s);

/* Run the full CMA-ES-lite loop (30 generations) and compute
 * the Pareto front over the union of all evaluated individuals.
 * Returns the number of non-dominated points. */
int cos_v163_search_run(cos_v163_search_t *s);

/* Produce an auto-profile under a device budget.  Picks the
 * Pareto point whose (latency, memory) both satisfy the budget
 * and whose accuracy is maximal among the remaining candidates. */
cos_v163_auto_profile_t cos_v163_auto_profile(const cos_v163_search_t *s,
                                              float latency_budget_ms,
                                              float memory_budget_mb);

/* JSON serializer (genes + Pareto + final population). */
size_t cos_v163_search_to_json(const cos_v163_search_t *s,
                               char *buf, size_t cap);

int cos_v163_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V163_EVOLVE_H */
