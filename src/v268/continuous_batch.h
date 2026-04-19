/*
 * v268 σ-Continuous-Batch — σ-priority continuous
 *                           batching.
 *
 *   Continuous batching streams requests into the
 *   serving engine without waiting for a static batch
 *   to fill.  v268 layers σ on top: priority is driven
 *   by σ_difficulty, preemption by σ_urgency, batch
 *   size by σ_load, and cost by σ_cost.
 *
 *   v0 manifests (strict, enumerated):
 *
 *   Priority queue (exactly 6 requests):
 *     Each row: `req`, `sigma_difficulty`, `engine`,
 *     `priority_slot` (1 = served first).  Rule:
 *     priority_slot is ascending order of
 *     sigma_difficulty (low σ first — fast path).
 *     Contract: priority_slot is a permutation of
 *     [1..6] AND matches argsort(+sigma_difficulty).
 *
 *   Preemption (exactly 2 scenarios):
 *     low-prio running + high-prio arrives →
 *       preempted=true, winner = high-prio id.
 *     equal-prio + new arrives →
 *       preempted=false, winner = incumbent id.
 *     Contract: both outcomes fire (≥ 1 true AND
 *     ≥ 1 false).
 *
 *   Adaptive batch size (exactly 3 load levels,
 *   canonical):
 *     low    : sigma_load=0.15 → batch_size=4
 *     medium : sigma_load=0.45 → batch_size=16
 *     high   : sigma_load=0.80 → batch_size=64
 *     Rule: batch_size is strictly monotone-increasing
 *     in sigma_load.  Contract: level sequence
 *     canonical AND batch_size strictly ascending.
 *
 *   Cost-aware routing (exactly 2 scenarios):
 *     scenario "local_free":  engine=local,  cost_eur=0
 *     scenario "api_paid":    engine=cloud,  cost_eur>0
 *     sigma_cost == (cost_eur == 0) ? 0 : >0
 *     Rule: for each scenario the chosen engine
 *     minimises cost under the routing policy
 *     (local preferred when σ_difficulty allows).
 *     Contract: both scenarios fire AND total local
 *     cost < total api cost.
 *
 *   σ_continuous_batch (surface hygiene):
 *       σ_continuous_batch = 1 −
 *         (queue_ok + queue_order_ok + preempt_ok +
 *          preempt_branches_ok + batch_rows_ok +
 *          batch_monotone_ok + cost_rows_ok +
 *          cost_total_ok) /
 *         (6 + 1 + 2 + 1 + 3 + 1 + 2 + 1)
 *   v0 requires `σ_continuous_batch == 0.0`.
 *
 *   Contracts (v0):
 *     1. Exactly 6 queue entries; priority_slot
 *        permutation of [1..6] AND matches
 *        argsort(+sigma_difficulty).
 *     2. Exactly 2 preemption rows; both true and
 *        false outcomes fire.
 *     3. Exactly 3 load levels canonical; batch_size
 *        strictly ascending in sigma_load.
 *     4. Exactly 2 cost scenarios; total local cost <
 *        total api cost.
 *     5. σ_continuous_batch ∈ [0, 1] AND == 0.0.
 *     6. FNV-1a chain replays byte-identically.
 *
 *   v268.1 (named, not implemented): live queue wired
 *     to v262 hybrid engine running real preemption,
 *     measured latency + throughput under load, cost
 *     tracker emitting real €/month figures.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V268_CONTINUOUS_BATCH_H
#define COS_V268_CONTINUOUS_BATCH_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V268_N_QUEUE    6
#define COS_V268_N_PREEMPT  2
#define COS_V268_N_LEVELS   3
#define COS_V268_N_COST     2

typedef struct {
    char  req[16];
    float sigma_difficulty;
    char  engine[16];
    int   priority_slot;  /* 1..6, lower served first */
} cos_v268_queue_t;

typedef struct {
    char  scenario[20];
    float sigma_urgency_incumbent;
    float sigma_urgency_arrival;
    bool  preempted;
    char  winner[16];
} cos_v268_preempt_t;

typedef struct {
    char  level[8];       /* "low"/"medium"/"high" */
    float sigma_load;
    int   batch_size;
} cos_v268_batch_level_t;

typedef struct {
    char  scenario[16];
    char  engine[16];
    float cost_eur;
    float sigma_cost;
} cos_v268_cost_t;

typedef struct {
    cos_v268_queue_t        queue    [COS_V268_N_QUEUE];
    cos_v268_preempt_t      preempt  [COS_V268_N_PREEMPT];
    cos_v268_batch_level_t  levels   [COS_V268_N_LEVELS];
    cos_v268_cost_t         costs    [COS_V268_N_COST];

    int   n_queue_ok;
    bool  queue_order_ok;
    int   n_preempt_ok;
    int   n_preempt_true;
    int   n_preempt_false;
    int   n_levels_ok;
    bool  batch_monotone_ok;
    int   n_costs_ok;
    float total_local_eur;
    float total_api_eur;

    float sigma_continuous_batch;

    bool     chain_valid;
    uint64_t terminal_hash;
    uint64_t seed;
} cos_v268_state_t;

void   cos_v268_init(cos_v268_state_t *s, uint64_t seed);
void   cos_v268_run (cos_v268_state_t *s);

size_t cos_v268_to_json(const cos_v268_state_t *s,
                         char *buf, size_t cap);

int    cos_v268_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V268_CONTINUOUS_BATCH_H */
