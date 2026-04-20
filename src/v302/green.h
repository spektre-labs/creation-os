/*
 * v302 σ-Green — energy-aware inference.
 *
 *   AI inference is projected to cross 1000 TWh a
 *   year.  A σ-gate that abstains 20% of the time
 *   saves 20% of the energy of those 20% of calls —
 *   abstention is green by construction.  v302 types
 *   four v0 manifests for that: a σ-driven compute
 *   budget, carbon-aware scheduling, an abstain-as-
 *   savings ledger, and a new metric — joules per
 *   *reliable* token — that rewards systems for
 *   filtering noise instead of spraying it.
 *
 *   v0 manifests (strict, enumerated):
 *
 *   σ-aware compute budget (exactly 3 canonical tiers):
 *     `easy`    σ_difficulty=0.10 SMALL   0.5 J;
 *     `medium`  σ_difficulty=0.40 MID     2.0 J;
 *     `hard`    σ_difficulty=0.80 LARGE   8.0 J.
 *     Contract: 3 rows in order; σ_difficulty
 *     strictly increasing; energy_j strictly
 *     increasing; exactly 3 DISTINCT model tiers.
 *
 *   Carbon-aware scheduling (exactly 3 canonical
 *   requests):
 *     `urgent_green`       urgency=HIGH grid=GREEN
 *                          → processed=true;
 *     `low_urgency_brown`  urgency=LOW  grid=BROWN
 *                          → processed=false
 *                          (defer for green window);
 *     `urgent_brown`       urgency=HIGH grid=BROWN
 *                          → processed=true
 *                          (latency wins).
 *     Contract: `processed = (urgency==HIGH) OR
 *     (grid==GREEN)` on every row; both processed
 *     branches fire.
 *
 *   Abstain = energy savings (exactly 3 canonical
 *   ledger rows):
 *     `baseline`           total=1000  abstained=0
 *                          energy_j=100  saved=0.00;
 *     `sigma_gated_light`  total=1000  abstained=200
 *                          energy_j=80   saved=0.20;
 *     `sigma_gated_heavy`  total=1000  abstained=500
 *                          energy_j=50   saved=0.50.
 *     Contract: abstained / total matches saved_ratio
 *     within 1e-3; saved_ratio strictly increasing;
 *     energy_j strictly decreasing.
 *
 *   Joules per reliable token (exactly 3 canonical
 *   regimes):
 *     `unfiltered`    J=100  reliable=700  noisy=300
 *                     J_per_reliable≈0.143;
 *     `soft_filter`   J=80   reliable=700  noisy=0
 *                     J_per_reliable≈0.114;
 *     `hard_filter`   J=70   reliable=700  noisy=0
 *                     J_per_reliable=0.100.
 *     Contract: J_per_reliable matches
 *     J / reliable within 1e-3; strictly decreasing
 *     across the 3 rows.
 *
 *   σ_green (surface hygiene):
 *      σ_green = 1 − (
 *          budget_rows_ok + budget_order_ok +
 *            budget_distinct_ok +
 *          sched_rows_ok + sched_rule_ok +
 *            sched_polarity_ok +
 *          save_rows_ok + save_ratio_ok +
 *            save_order_ok + save_energy_decrease_ok +
 *          jpt_rows_ok + jpt_formula_ok +
 *            jpt_decrease_ok
 *      ) / (3 + 1 + 1 + 3 + 1 + 1 +
 *           3 + 1 + 1 + 1 + 3 + 1 + 1)
 *   v0 requires `σ_green == 0.0`.
 *
 *   Contracts (v0):
 *     1. 3 budget tiers; σ ↑, J ↑, 3 distinct tiers.
 *     2. 3 schedule rows; rule holds on every row;
 *        both processed branches fire.
 *     3. 3 savings rows; saved_ratio = abstained/
 *        total; saved ↑; energy ↓.
 *     4. 3 J/reliable regimes; J_per_reliable =
 *        J/reliable; strictly ↓.
 *     5. σ_green ∈ [0, 1] AND == 0.0.
 *     6. FNV-1a chain replays byte-identically.
 *
 *   v302.1 (named, not implemented): live carbon-
 *     intensity feeds from a national grid API gated
 *     by v293 hagia-sofia continuous-use and a
 *     per-deployment `J_per_reliable_token` receipt
 *     emitted by the v282 σ-agent runtime.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V302_GREEN_H
#define COS_V302_GREEN_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V302_N_BUDGET 3
#define COS_V302_N_SCHED  3
#define COS_V302_N_SAVE   3
#define COS_V302_N_JPT    3

typedef struct {
    char  difficulty[8];
    float sigma_difficulty;
    char  model_tier[8];
    float energy_j;
} cos_v302_budget_t;

typedef struct {
    char label[20];
    char urgency[6];
    char grid[8];
    bool processed;
} cos_v302_sched_t;

typedef struct {
    char  regime[20];
    int   total_tokens;
    int   abstained_tokens;
    float energy_j;
    float saved_ratio;
} cos_v302_save_t;

typedef struct {
    char  regime[14];
    float energy_j;
    int   reliable_tokens;
    int   noisy_tokens;
    float j_per_reliable;
} cos_v302_jpt_t;

typedef struct {
    cos_v302_budget_t budget[COS_V302_N_BUDGET];
    cos_v302_sched_t  sched [COS_V302_N_SCHED];
    cos_v302_save_t   save  [COS_V302_N_SAVE];
    cos_v302_jpt_t    jpt   [COS_V302_N_JPT];

    int  n_budget_rows_ok;
    bool budget_order_ok;
    bool budget_distinct_ok;

    int  n_sched_rows_ok;
    bool sched_rule_ok;
    bool sched_polarity_ok;

    int  n_save_rows_ok;
    bool save_ratio_ok;
    bool save_order_ok;
    bool save_energy_decrease_ok;

    int  n_jpt_rows_ok;
    bool jpt_formula_ok;
    bool jpt_decrease_ok;

    float sigma_green;

    bool     chain_valid;
    uint64_t terminal_hash;
    uint64_t seed;
} cos_v302_state_t;

void   cos_v302_init(cos_v302_state_t *s, uint64_t seed);
void   cos_v302_run (cos_v302_state_t *s);
size_t cos_v302_to_json(const cos_v302_state_t *s,
                         char *buf, size_t cap);
int    cos_v302_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V302_GREEN_H */
