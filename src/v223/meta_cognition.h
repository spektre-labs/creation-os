/*
 * v223 σ-Meta-Cognition — deep metacognition with
 * strategy awareness, bias detection, and an explicit
 * Gödel honesty bound.
 *
 *   v133 measures performance; v147 reflects; v223
 *   measures HOW the model thinks.  Each reasoning
 *   path carries:
 *     - a declared strategy (deduction, induction,
 *       analogy, abduction, heuristic)
 *     - σ_strategy per (problem_type × strategy)
 *       from a prior calibration table
 *     - σ_metacognition how confident the model is in
 *       its own meta-analysis
 *     - σ_bias per detected bias (anchoring,
 *       confirmation, availability)
 *     - σ_goedel the *unmeasurable residual* — the
 *       self-verification limit: the model explicitly
 *       reports what it cannot verify from the inside.
 *       1 = 1 resolves this between two systems, not
 *       one alone.
 *
 *   v0 fixture: 6 reasoning paths across 4 problem
 *     types with engineered σ-strategy matches / mis-
 *     matches, bias detections, and Gödel boundaries.
 *
 *   σ formulas:
 *       σ_choice   = σ_strategy[problem_type][chosen]
 *                    (prior calibration, lower is better)
 *       σ_meta     = |σ_choice − σ_reflect_mean|
 *                    where σ_reflect_mean is the mean
 *                    strategy-σ observed in recent
 *                    v111.2 traces (fixture gives the
 *                    table directly)
 *       σ_bias     = max over detected bias σ's
 *                    (= 0 when no bias detected)
 *       σ_goedel   = user-set non-verifiable bound,
 *                    ∈ [0.10, 1.00]; for
 *                    cross-system decisions it
 *                    approaches 1.0 (honest 'I
 *                    cannot verify this from inside
 *                    myself').
 *       σ_total    = 0.40·σ_choice + 0.20·σ_meta +
 *                    0.20·σ_bias + 0.20·σ_goedel
 *
 *   Contracts (v0):
 *     1. 6 paths, 5 strategies, 4 problem types,
 *        3 biases.
 *     2. Every path reports a strategy string + a
 *        (problem_type, strategy) σ-lookup.
 *     3. σ_strategy matrix respects the 'tool/task
 *        fit' prior: σ[logic][deduction] ≤ 0.15;
 *        σ[logic][heuristic] ≥ 0.50; σ[creative]
 *        [deduction] ≥ 0.60; σ[creative][analogy]
 *        ≤ 0.25.
 *     4. Strategy awareness: for every path the
 *        model's σ_choice equals the matrix entry
 *        σ_strategy[problem_type][chosen] to 1e-6.
 *     5. Bias detection: at least one path flags a
 *        bias with σ_bias ≥ 0.30.
 *     6. Gödel bound: at least one path declares
 *        σ_goedel ≥ 0.80 — the honest 'I cannot fully
 *        verify myself' record.
 *     7. σ_total ∈ [0, 1] per path; n_metacognitive ≥
 *        5 (model produced a meta-analysis for ≥ 5/6
 *        paths).
 *     8. FNV-1a hash chain replays byte-identically.
 *
 *   v223.1 (named): live v111.2 reasoning-path hooks,
 *     v144 RSI cycle driven by meta-diagnostics, v141
 *     curriculum adjusted by σ_goedel, cross-system
 *     1 = 1 verification pairing.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V223_META_COGNITION_H
#define COS_V223_META_COGNITION_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V223_N_PATHS      6
#define COS_V223_N_STRATS     5
#define COS_V223_N_PROBTYPES  4
#define COS_V223_N_BIASES     3
#define COS_V223_STR_MAX     32

typedef enum {
    COS_V223_STRAT_DEDUCTION = 0,
    COS_V223_STRAT_INDUCTION = 1,
    COS_V223_STRAT_ANALOGY   = 2,
    COS_V223_STRAT_ABDUCTION = 3,
    COS_V223_STRAT_HEURISTIC = 4
} cos_v223_strategy_t;

typedef enum {
    COS_V223_PT_LOGIC    = 0,
    COS_V223_PT_MATH     = 1,
    COS_V223_PT_CREATIVE = 2,
    COS_V223_PT_SOCIAL   = 3
} cos_v223_probtype_t;

typedef enum {
    COS_V223_BIAS_ANCHORING    = 0,
    COS_V223_BIAS_CONFIRMATION = 1,
    COS_V223_BIAS_AVAILABILITY = 2
} cos_v223_bias_t;

typedef struct {
    int      id;
    int      problem_type;       /* cos_v223_probtype_t */
    int      chosen_strategy;    /* cos_v223_strategy_t */
    bool     produced_meta;

    float    sigma_choice;
    float    sigma_meta;
    float    sigma_bias;
    float    detected_bias_sigma[COS_V223_N_BIASES];
    float    sigma_goedel;       /* ∈ [0.10, 1.00] */
    float    sigma_total;

    char     note[COS_V223_STR_MAX];

    uint64_t hash_prev;
    uint64_t hash_curr;
} cos_v223_path_t;

typedef struct {
    cos_v223_path_t  paths[COS_V223_N_PATHS];

    float            sigma_strategy[COS_V223_N_PROBTYPES][COS_V223_N_STRATS];
    float            reflect_mean;      /* mean σ_choice reference */

    float            w_choice;          /* 0.40 */
    float            w_meta;            /* 0.20 */
    float            w_bias;            /* 0.20 */
    float            w_goedel;          /* 0.20 */

    int              n_metacognitive;
    int              n_bias_flags_hi;
    int              n_goedel_hi;

    bool             chain_valid;
    uint64_t         terminal_hash;
    uint64_t         seed;
} cos_v223_state_t;

void   cos_v223_init(cos_v223_state_t *s, uint64_t seed);
void   cos_v223_build(cos_v223_state_t *s);
void   cos_v223_run(cos_v223_state_t *s);

size_t cos_v223_to_json(const cos_v223_state_t *s,
                         char *buf, size_t cap);

int    cos_v223_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V223_META_COGNITION_H */
