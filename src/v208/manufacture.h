/*
 * v208 σ-Manufacture — digital manufacturing pipeline with
 * DFM analysis, process simulation, quality prediction,
 * and a closed loop back to v204.
 *
 *   Input: v207 Pareto designs → manufacturing instances.
 *   v0:
 *     - N = 4 designs → process plan (4 stages each)
 *     - DFM analysis: 5 canonical rules
 *         TOL_TOO_TIGHT, MATERIAL_COST, ASSEMBLY_COMPLEX,
 *         PART_COUNT_HIGH, SPECIAL_PROCESS
 *       each producing σ_dfm per violation.
 *     - Process simulation: σ_process per stage.
 *     - Quality prediction: σ_quality per design with
 *       extra checkpoints when σ_quality > τ_quality
 *       (v159 heal semantics applied to manufacture).
 *     - Closed loop: every manufacturing run emits a
 *       "feedback hypothesis id" that feeds the next
 *       v204 generation, closing the
 *         hypothesis → experiment → theorem → design →
 *         manufacture → hypothesis
 *       cycle.
 *
 *   Contracts (v0):
 *     1. 4 designs manufactured.
 *     2. DFM analysis flags ≥ 1 issue across the fleet.
 *     3. σ_dfm, σ_process, σ_quality ∈ [0,1].
 *     4. High-σ_quality designs get extra checkpoints
 *        (n_checkpoints increases monotonically with σ).
 *     5. Every design emits a non-zero feedback hypothesis
 *        id (the closed loop is mandatory).
 *     6. FNV-1a hash chain byte-deterministic.
 *
 * v208.1 (named): BOM generator, live v176 process
 *   simulator, v159 heal online rework, v181 audit.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V208_MANUFACTURE_H
#define COS_V208_MANUFACTURE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V208_N_DESIGNS   4
#define COS_V208_N_STAGES    4
#define COS_V208_N_DFM_RULES 5

typedef enum {
    COS_V208_DFM_TOL_TOO_TIGHT     = 0,
    COS_V208_DFM_MATERIAL_COST     = 1,
    COS_V208_DFM_ASSEMBLY_COMPLEX  = 2,
    COS_V208_DFM_PART_COUNT_HIGH   = 3,
    COS_V208_DFM_SPECIAL_PROCESS   = 4
} cos_v208_dfm_rule_t;

typedef struct {
    int      rule;          /* cos_v208_dfm_rule_t */
    float    sigma_dfm;
    bool     flagged;
    int      suggestion_id; /* non-zero if auto-fix available */
} cos_v208_dfm_finding_t;

typedef struct {
    int      id;
    int      design_id;                /* from v207 */
    int      n_parts;
    int      n_stages;
    cos_v208_dfm_finding_t dfm[COS_V208_N_DFM_RULES];
    int      n_dfm_flagged;
    float    sigma_process[COS_V208_N_STAGES];
    float    sigma_process_max;
    float    sigma_quality;
    int      n_checkpoints;            /* v159-style heal */
    int      feedback_hypothesis_id;   /* closed-loop v204 */
    uint64_t hash_prev;
    uint64_t hash_curr;
} cos_v208_run_t;

typedef struct {
    int               n;
    cos_v208_run_t    runs[COS_V208_N_DESIGNS];

    float             tau_quality;    /* 0.40 */
    float             tau_dfm;        /* 0.50 */

    int               total_dfm_flagged;
    int               total_checkpoints;

    bool              chain_valid;
    uint64_t          terminal_hash;
    uint64_t          seed;
} cos_v208_state_t;

void   cos_v208_init(cos_v208_state_t *s, uint64_t seed);
void   cos_v208_build(cos_v208_state_t *s);
void   cos_v208_run(cos_v208_state_t *s);

size_t cos_v208_to_json(const cos_v208_state_t *s,
                         char *buf, size_t cap);

int    cos_v208_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V208_MANUFACTURE_H */
