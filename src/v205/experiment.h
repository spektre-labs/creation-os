/*
 * v205 σ-Experiment — automated experimental design for
 * v204 hypotheses.
 *
 *   For each of the 3 TEST_QUEUE hypotheses from v204 v205
 *   builds an experiment descriptor:
 *     - dependent / independent / control variable ids
 *     - σ_design: is the experiment structurally valid?
 *     - σ_sim   : reliability of the cheap simulation-first
 *                 run (v176)
 *     - σ_power : does the required n fit the budget?
 *     - n_required: closed-form from effect size + α + β
 *     - run_decision: SIM_SUPPORTS, SIM_REFUTES, UNDER_POWERED
 *     - repro receipt (hash of the full record)
 *
 *   Simulation-first is the core contract: the cheap sim
 *   runs before the expensive real experiment.  Only
 *   hypotheses where σ_sim < τ_sim and the sim result
 *   supports the hypothesis graduate to RUN_REAL.  This
 *   reproduces the "save resources on bad experiments"
 *   path explicitly.
 *
 *   Contracts (v0):
 *     1. N_EXP = 3 experiments (one per v204 queue slot).
 *     2. Each experiment has exactly 3 distinct variable
 *        roles (dep / indep / control) — structural
 *        validity.
 *     3. σ_design, σ_sim, σ_power ∈ [0,1].
 *     4. n_required deterministic from (effect_size, α, β)
 *        using the canonical formula
 *             n = (z_alpha + z_beta)^2 / effect^2
 *     5. Exactly one run_decision per experiment; at least
 *        one UNDER_POWERED and at least one SIM_SUPPORTS in
 *        the v0 fixture.
 *     6. Repro chain valid + byte-deterministic — this
 *        *is* the `cos reproduce --experiment` spine.
 *
 * v205.1 (named): live v121 planner, v176 simulator hook,
 *   v181 streaming audit.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V205_EXPERIMENT_H
#define COS_V205_EXPERIMENT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V205_N_EXP      3
#define COS_V205_N_VARS     3
#define COS_V205_BUDGET_N  500

typedef enum {
    COS_V205_VAR_DEPENDENT   = 0,
    COS_V205_VAR_INDEPENDENT = 1,
    COS_V205_VAR_CONTROL     = 2
} cos_v205_var_role_t;

typedef enum {
    COS_V205_DEC_SIM_SUPPORTS  = 0,
    COS_V205_DEC_SIM_REFUTES   = 1,
    COS_V205_DEC_UNDER_POWERED = 2
} cos_v205_decision_t;

typedef struct {
    int      id;
    int      hypothesis_id;        /* id from v204 */
    int      dep_var;
    int      indep_var;
    int      control_var;
    float    effect_size;          /* e.g. 0.20 */
    float    alpha;                /* 0.05 */
    float    beta;                 /* 0.20 → power 0.80 */
    int      n_required;
    bool     fits_budget;
    float    sigma_design;
    float    sigma_sim;
    float    sigma_power;
    int      decision;             /* cos_v205_decision_t */
    uint64_t hash_prev;
    uint64_t hash_curr;
} cos_v205_experiment_t;

typedef struct {
    int                    n;
    cos_v205_experiment_t  exp[COS_V205_N_EXP];

    float                  tau_sim;        /* 0.35 */
    float                  tau_power_sigma;/* 0.40 */
    int                    budget_n;       /* 500 */

    int                    n_sim_supports;
    int                    n_sim_refutes;
    int                    n_under_powered;

    bool                   chain_valid;
    uint64_t               terminal_hash;
    uint64_t               seed;
} cos_v205_state_t;

void   cos_v205_init(cos_v205_state_t *s, uint64_t seed);
void   cos_v205_build(cos_v205_state_t *s);
void   cos_v205_run(cos_v205_state_t *s);

size_t cos_v205_to_json(const cos_v205_state_t *s,
                         char *buf, size_t cap);

int    cos_v205_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V205_EXPERIMENT_H */
