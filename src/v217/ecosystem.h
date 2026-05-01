/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/*
 * v217 σ-Ecosystem — whole-system health with trophic
 * levels, resource flow, and symbiosis.
 *
 *   An ecosystem is healthy when no one level dominates
 *   the others.  v217 models the Creation OS agent
 *   population in four trophic levels:
 *
 *       0  PRODUCERS  (v174 flywheel, data generators)
 *       1  CONSUMERS  (v101 inference, users of data)
 *       2  DECOMPOSERS(v177 compress + v159 heal)
 *       3  PREDATORS  (v122 red-team + v210 guardian)
 *
 *   Resource flow (compute / memory / bandwidth) is
 *   allocated by v200 market.  Symbiosis is tracked as
 *   named pairs:
 *
 *       MUTUAL     v120 distill: big model teaches small
 *       COMPETE    v150 debate: agents challenge each other
 *       COMMENSAL  v215 stigmergy: indirect cooperation
 *
 *   σ_ecosystem aggregates:
 *     σ_dominance — max share over N_LEVELS; ≥ τ_dom
 *                   (0.40) means a level dominates.
 *     σ_balance   — 1 − min share / max share; high = unbalanced.
 *     σ_symbiosis — per-pair σ averaged.
 *     σ_ecosystem = 0.4·σ_dominance + 0.4·σ_balance + 0.2·σ_symbiosis
 *
 *   K_eff (v193-style): 1 − σ_ecosystem  ∈ [0, 1].
 *
 *   Contracts (v0):
 *     1. 4 trophic levels present with populations
 *        summing to POP_TOTAL = 100.
 *     2. No level share exceeds τ_dom (0.40) — trophic
 *        balance enforced.
 *     3. ≥ 3 symbiotic pairs recorded; each σ_pair
 *        ∈ [0, 1].
 *     4. σ_ecosystem ∈ [0, 1] and K_eff ∈ [0, 1].
 *     5. K_eff > τ_healthy (0.55) — the v0 fixture is
 *        a *healthy* snapshot.
 *     6. FNV-1a chain over (levels, pairs, aggregate)
 *        replays byte-identically.
 *
 *   v217.1 (named): live v174 / v101 / v177 / v159 /
 *     v122 / v210 head-counts, v200 market allocation,
 *     v193 coherence dashboard.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V217_ECOSYSTEM_H
#define COS_V217_ECOSYSTEM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V217_N_LEVELS 4
#define COS_V217_N_PAIRS  5
#define COS_V217_STR_MAX  24

typedef enum {
    COS_V217_LVL_PRODUCER   = 0,
    COS_V217_LVL_CONSUMER   = 1,
    COS_V217_LVL_DECOMPOSER = 2,
    COS_V217_LVL_PREDATOR   = 3
} cos_v217_level_t;

typedef enum {
    COS_V217_SYM_MUTUAL    = 0,
    COS_V217_SYM_COMPETE   = 1,
    COS_V217_SYM_COMMENSAL = 2
} cos_v217_sym_t;

typedef struct {
    int      id;                    /* cos_v217_level_t */
    char     name[COS_V217_STR_MAX];
    int      population;
    float    compute_share;         /* ∈ [0,1] */
    float    share;                 /* population / total */
} cos_v217_trophic_t;

typedef struct {
    int      id;
    int      kind;                  /* cos_v217_sym_t */
    char     a[COS_V217_STR_MAX];
    char     b[COS_V217_STR_MAX];
    float    sigma_pair;
} cos_v217_symbiosis_t;

typedef struct {
    cos_v217_trophic_t   levels[COS_V217_N_LEVELS];
    cos_v217_symbiosis_t pairs [COS_V217_N_PAIRS];

    int                  pop_total;

    float                tau_dom;        /* 0.40 */
    float                tau_healthy;    /* 0.55 */

    float                sigma_dominance;
    float                sigma_balance;
    float                sigma_symbiosis;
    float                sigma_ecosystem;
    float                k_eff;

    bool                 chain_valid;
    uint64_t             terminal_hash;
    uint64_t             seed;
} cos_v217_state_t;

void   cos_v217_init(cos_v217_state_t *s, uint64_t seed);
void   cos_v217_build(cos_v217_state_t *s);
void   cos_v217_run(cos_v217_state_t *s);

size_t cos_v217_to_json(const cos_v217_state_t *s,
                         char *buf, size_t cap);

int    cos_v217_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V217_ECOSYSTEM_H */
