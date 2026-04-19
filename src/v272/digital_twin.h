/*
 * v272 σ-Digital-Twin — physical + digital kept in sync
 *                        by σ.
 *
 *   A twin is a physical system and a digital copy.
 *   v272 types four sub-manifests: twin sync (σ_twin
 *   measures drift between physical and simulated),
 *   predictive maintenance (σ_prediction decides
 *   replace vs monitor), what-if (σ_whatif decides
 *   implement vs abort), and verified action
 *   (declared == realized, gated by σ_match).
 *
 *   v0 manifests (strict, enumerated):
 *
 *   Twin sync (exactly 4 fixtures):
 *     Each: `timestamp_s`, `σ_twin ∈ [0, 1]`,
 *     `stable == (σ_twin < 0.05)`,
 *     `drifted == (σ_twin > 0.30)`.
 *     Contract: ≥ 1 stable AND ≥ 1 drifted fires.
 *
 *   Predictive maintenance (exactly 3 rows):
 *     Each: `part_id`, `predicted_failure_hours > 0`,
 *     `σ_prediction ∈ [0, 1]`,
 *     `action ∈ {REPLACE, MONITOR}`,
 *     rule: `σ_prediction ≤ τ_pred = 0.30 →
 *            REPLACE` else → `MONITOR`.
 *     Contract: ≥ 1 REPLACE AND ≥ 1 MONITOR.
 *
 *   What-if scenarios (exactly 3 rows):
 *     Each: `label`, `delta_param_pct`,
 *     `σ_whatif ∈ [0, 1]`,
 *     `decision ∈ {IMPLEMENT, ABORT}`,
 *     rule: `σ_whatif ≤ τ_whatif = 0.25 →
 *            IMPLEMENT` else → `ABORT`.
 *     Contract: ≥ 1 IMPLEMENT AND ≥ 1 ABORT.
 *
 *   Verified action (exactly 3 rows):
 *     Each: `action_id`, `σ_match ∈ [0, 1]`,
 *     `declared_sim ∈ [0, 1]`,
 *     `realized_phys ∈ [0, 1]`,
 *     rule: `σ_match == |declared_sim −
 *            realized_phys|` (one-to-one typing of
 *            declared = realized),
 *     `verdict ∈ {PASS, BLOCK}`,
 *     rule: `σ_match ≤ τ_match = 0.10 → PASS`
 *           else → `BLOCK`.
 *     Contract: ≥ 1 PASS AND ≥ 1 BLOCK.
 *
 *   σ_digital_twin (surface hygiene):
 *       σ_digital_twin = 1 −
 *         (sync_rows_ok + sync_both_ok +
 *          pred_rows_ok + pred_both_ok +
 *          whatif_rows_ok + whatif_both_ok +
 *          verified_rows_ok + verified_both_ok) /
 *         (4 + 1 + 3 + 1 + 3 + 1 + 3 + 1)
 *   v0 requires `σ_digital_twin == 0.0`.
 *
 *   Contracts (v0):
 *     1. 4 sync rows; stable/drifted flags match
 *        σ_twin; both fire.
 *     2. 3 maintenance rows; action matches σ_pred vs
 *        τ_pred; both fire.
 *     3. 3 what-if rows; decision matches σ_whatif vs
 *        τ_whatif; both fire.
 *     4. 3 verified-action rows; σ_match ==
 *        |declared − realized|; verdict matches τ_match;
 *        both fire.
 *     5. σ_digital_twin ∈ [0, 1] AND == 0.0.
 *     6. FNV-1a chain replays byte-identically.
 *
 *   v272.1 (named, not implemented): live sensor
 *     feed into v220 simulator, continuous σ_twin
 *     calibration, predictive-maintenance receipts
 *     signed via v181 audit + v234 presence, live
 *     what-if feeding v148 sovereign decisions.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V272_DIGITAL_TWIN_H
#define COS_V272_DIGITAL_TWIN_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V272_N_SYNC      4
#define COS_V272_N_PRED      3
#define COS_V272_N_WHATIF    3
#define COS_V272_N_VERIFIED  3

typedef enum {
    COS_V272_MAINT_REPLACE = 1,
    COS_V272_MAINT_MONITOR = 2
} cos_v272_maint_t;

typedef enum {
    COS_V272_WHATIF_IMPLEMENT = 1,
    COS_V272_WHATIF_ABORT     = 2
} cos_v272_whatif_t;

typedef enum {
    COS_V272_VERDICT_PASS  = 1,
    COS_V272_VERDICT_BLOCK = 2
} cos_v272_verdict_t;

typedef struct {
    int    timestamp_s;
    float  sigma_twin;
    bool   stable;
    bool   drifted;
} cos_v272_sync_t;

typedef struct {
    char             part_id[12];
    float            predicted_failure_hours;
    float            sigma_prediction;
    cos_v272_maint_t action;
} cos_v272_pred_t;

typedef struct {
    char               label[16];
    float              delta_param_pct;
    float              sigma_whatif;
    cos_v272_whatif_t  decision;
} cos_v272_whatif_row_t;

typedef struct {
    char                action_id[12];
    float               declared_sim;
    float               realized_phys;
    float               sigma_match;
    cos_v272_verdict_t  verdict;
} cos_v272_verified_t;

typedef struct {
    cos_v272_sync_t        sync    [COS_V272_N_SYNC];
    cos_v272_pred_t        pred    [COS_V272_N_PRED];
    cos_v272_whatif_row_t  whatif  [COS_V272_N_WHATIF];
    cos_v272_verified_t    verified[COS_V272_N_VERIFIED];

    float tau_pred;    /* 0.30 */
    float tau_whatif;  /* 0.25 */
    float tau_match;   /* 0.10 */
    float stable_thr;  /* 0.05 */
    float drift_thr;   /* 0.30 */

    int   n_sync_rows_ok;
    int   n_sync_stable;
    int   n_sync_drifted;
    int   n_pred_rows_ok;
    int   n_pred_replace;
    int   n_pred_monitor;
    int   n_whatif_rows_ok;
    int   n_whatif_implement;
    int   n_whatif_abort;
    int   n_verified_rows_ok;
    int   n_verified_pass;
    int   n_verified_block;

    float sigma_digital_twin;

    bool     chain_valid;
    uint64_t terminal_hash;
    uint64_t seed;
} cos_v272_state_t;

void   cos_v272_init(cos_v272_state_t *s, uint64_t seed);
void   cos_v272_run (cos_v272_state_t *s);

size_t cos_v272_to_json(const cos_v272_state_t *s,
                         char *buf, size_t cap);

int    cos_v272_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V272_DIGITAL_TWIN_H */
