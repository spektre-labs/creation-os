/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/*
 * v220 σ-Simulate — domain-agnostic Monte Carlo with
 * what-if and σ-per-rule confidence.
 *
 *   v149 embodied is MuJoCo-specific; v176 simulator
 *   generates synthetic worlds; v220 is the general
 *   engine.  Input: a typed rule set + an initial
 *   state.  Output: a distribution of terminal states
 *   with σ-per-transition confidence and a what-if
 *   comparison against a perturbed rule set.
 *
 *   Rule shape (v135-Prolog-style, deterministic here):
 *       transition(state, action, next_state)
 *         :- condition_fn(state), σ_rule < τ_rule.
 *
 *   In v0 we model a 4-state Markov-ish system over
 *   200 Monte Carlo rollouts of 8 steps each, under
 *   two rule sets:
 *       baseline  — nominal transition matrix
 *       whatif    — one rule perturbed
 *
 *   Monte Carlo produces a distribution over terminal
 *   states; σ_simulation is derived from the variance
 *   (normalised Shannon-entropy proxy over the
 *   terminal-state histogram).  Domain is deliberately
 *   untyped: the same engine handles physics, economy,
 *   ecology, or a game — we check the *shape*, not the
 *   content.
 *
 *   σ-metrics:
 *       σ_rule         per-rule confidence, ∈ [0,1]
 *       σ_sim_baseline normalised entropy over terminal
 *                       histogram under baseline
 *       σ_sim_whatif   same under whatif
 *       σ_causal       |Δ baseline vs whatif| per
 *                       terminal state (v140-style
 *                       attribution)
 *       σ_engine       max σ_rule across the active rule set
 *
 *   Contracts (v0):
 *     1. 4 rules each in baseline and whatif; σ_rule ≤ τ_rule.
 *     2. N_MC = 200 rollouts per scenario; 8 steps each.
 *     3. Terminal-state histogram sums to N_MC.
 *     4. σ_sim_baseline and σ_sim_whatif ∈ [0, 1].
 *     5. σ_causal ∈ [0, 1], sum ≤ 2 (bounded by
 *        max of the two distributions).
 *     6. Baseline and whatif histograms differ in at
 *        least one state (the perturbation really
 *        changes outcomes).
 *     7. σ_engine ≤ τ_rule so the simulation is
 *        *trusted* under both rule sets.
 *     8. FNV-1a hash chain over (rule set, histogram,
 *        σ-aggregate) replays byte-identically.
 *
 *   v220.1 (named): live v135 Prolog rule parser,
 *     v169 ontology for domain-specific typing,
 *     v140 per-rule causal attribution, v207 design
 *     inner-loop hook.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V220_SIMULATE_H
#define COS_V220_SIMULATE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V220_N_STATES   4
#define COS_V220_N_RULES    4
#define COS_V220_N_MC     200
#define COS_V220_N_STEPS    8
#define COS_V220_STR_MAX   24

typedef struct {
    int      id;
    char     name[COS_V220_STR_MAX];
    int      from_state;
    int      to_state;
    float    prob;       /* conditional transition prob */
    float    sigma_rule; /* ∈ [0, τ_rule] */
} cos_v220_rule_t;

typedef struct {
    cos_v220_rule_t  rules[COS_V220_N_RULES];
    int              histogram[COS_V220_N_STATES];
    float            sigma_sim;        /* ∈ [0, 1] */
} cos_v220_scenario_t;

typedef struct {
    cos_v220_scenario_t baseline;
    cos_v220_scenario_t whatif;

    int                 n_mc;
    int                 n_steps;
    int                 initial_state;

    float               tau_rule;      /* 0.10 */
    float               sigma_engine;  /* max σ_rule  */
    float               sigma_causal[COS_V220_N_STATES];

    bool                chain_valid;
    uint64_t            terminal_hash;
    uint64_t            seed;
} cos_v220_state_t;

void   cos_v220_init(cos_v220_state_t *s, uint64_t seed);
void   cos_v220_build(cos_v220_state_t *s);
void   cos_v220_run(cos_v220_state_t *s);

size_t cos_v220_to_json(const cos_v220_state_t *s,
                         char *buf, size_t cap);

int    cos_v220_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V220_SIMULATE_H */
