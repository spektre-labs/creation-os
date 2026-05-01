/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/*
 * v214 σ-Swarm-Evolve — evolutionary swarm intelligence.
 *
 *   v150 swarm debate is static: N fixed agents argue.
 *   v214 makes the swarm *evolve*.  Agents are born,
 *   compete, speciate, and are retired — with a single
 *   objective, non-subjective fitness function:
 *
 *       fitness = 1 / (σ_mean + ε)
 *
 *   The lower an agent's mean σ across a benchmark
 *   window, the fitter it is.  No human grader, no RLHF
 *   proxy — σ is the fitness.
 *
 *   v0 fixture: a 10-generation run of a 12-agent swarm
 *   across 4 ecological niches:
 *       NICHE_MATH  0
 *       NICHE_CODE  1
 *       NICHE_PROSE 2
 *       NICHE_BENCH 3
 *
 *   Generation step (deterministic):
 *     1. Every agent re-scores σ_mean on its niche.
 *     2. Bottom-2 globally are retired (die).
 *     3. Top-2 within each niche are bred pairwise
 *        (LoRA merge) to produce children; each child
 *        inherits its parents' niche.
 *     4. Speciation is tracked per-generation: an
 *        agent's `species_id` is its niche; a species
 *        is considered established once ≥ 2 agents
 *        carry it at end-of-generation.
 *
 *   Emergence (v192) is recorded as σ_emergent(g) —
 *   the fleet-wide σ_mean, which must decrease
 *   monotonically non-increasing across generations
 *   (the swarm gets better, not worse).
 *
 *   Contracts (v0):
 *     1. 10 generations recorded; 12 live agents at
 *        every step (deaths are replaced by births so
 *        population is constant).
 *     2. At least 3 niches reach "established" status
 *        (≥ 2 agents) at the final generation.
 *     3. σ_emergent(g) is monotone non-increasing —
 *        σ_emergent(g+1) ≤ σ_emergent(g) + 1e-6.
 *     4. At least one agent lineage spans ≥ 5
 *        generations (continuity).
 *     5. At least 8 agents total retired (lifecycle
 *        actually runs).
 *     6. FNV-1a hash chain over per-generation records
 *        replays byte-identically.
 *
 *   v214.1 (named): live LoRA merge via v125 PEFT
 *     adapters, v136 CMA-ES outer loop, v192 live
 *     emergence detection, v143 real benchmark
 *     re-score per generation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V214_SWARM_EVOLVE_H
#define COS_V214_SWARM_EVOLVE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V214_POP_SIZE       12
#define COS_V214_N_GENS         10
#define COS_V214_N_NICHES        4
#define COS_V214_MAX_AGENTS    128  /* includes retired */

typedef struct {
    int      agent_id;           /* monotonic */
    int      parent_a_id;        /* -1 if founder */
    int      parent_b_id;        /* -1 if founder */
    int      species_id;         /* = niche id in v0 */
    int      born_gen;
    int      retired_gen;        /* -1 if alive */
    float    sigma_mean;         /* current σ̄ */
    float    fitness;            /* 1 / (σ_mean + eps) */
} cos_v214_agent_t;

typedef struct {
    int      gen;
    float    sigma_emergent;     /* fleet-wide σ_mean */
    int      alive_count;
    int      births_this_gen;
    int      deaths_this_gen;
    int      established_species;
    int      per_niche_count[COS_V214_N_NICHES];
    uint64_t hash_prev;
    uint64_t hash_curr;
} cos_v214_gen_record_t;

typedef struct {
    int                       n_agents_total;
    cos_v214_agent_t          agents[COS_V214_MAX_AGENTS];

    int                       n_gens;
    cos_v214_gen_record_t     gens[COS_V214_N_GENS];

    float                     eps;            /* 1e-3 */
    int                       min_niche_pop;  /* 2 */

    int                       final_established_species;
    int                       max_lineage_span;
    int                       total_retired;
    float                     final_sigma_emergent;

    bool                      chain_valid;
    uint64_t                  terminal_hash;
    uint64_t                  seed;
} cos_v214_state_t;

void   cos_v214_init(cos_v214_state_t *s, uint64_t seed);
void   cos_v214_build(cos_v214_state_t *s);
void   cos_v214_run(cos_v214_state_t *s);

size_t cos_v214_to_json(const cos_v214_state_t *s,
                         char *buf, size_t cap);

int    cos_v214_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V214_SWARM_EVOLVE_H */
