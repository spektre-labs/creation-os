/*
 * v278 σ-Recursive-Self-Improve — σ learns to measure
 *                                  σ better.
 *
 *   The σ-gate itself is a manifest that improves over
 *   time.  v278 types four loops:
 *
 *   1. Calibration feedback (exactly 4 epochs):
 *      Each: `epoch ∈ [0..3]`, `sigma_calibration_err ∈
 *      [0, 1]`.
 *      Contract: strictly decreasing; last epoch ≤
 *      0.05.
 *
 *   2. Architecture search (exactly 3 configurations,
 *      canonical order):
 *      `n_channels ∈ {6, 8, 12}` with `aurcc ∈ [0, 1]`.
 *      Contract: `chosen` = argmax(aurcc) AND at least
 *      two configurations have distinct aurcc values
 *      (so a regression that ties everything fails).
 *
 *   3. Threshold adaptation (exactly 3 domains,
 *      canonical order):
 *      `code`, `creative`, `medical` each with tuned
 *      `tau ∈ (0, 1)`.
 *      Contract: `code.tau == 0.20`, `creative.tau ==
 *      0.50`, `medical.tau == 0.15`; strictly at least
 *      two distinct tau values (in fact three); every
 *      tau ∈ (0, 1).
 *
 *   4. Gödel awareness (exactly 3 fixtures,
 *      τ_goedel = 0.40):
 *      Each: `claim_id`, `sigma_goedel ∈ [0, 1]`,
 *      `action ∈ {SELF_CONFIDENT, CALL_PROCONDUCTOR}`,
 *      rule: `sigma_goedel ≤ τ_goedel → SELF_CONFIDENT`
 *      else CALL_PROCONDUCTOR.
 *      Contract: ≥ 1 SELF_CONFIDENT AND ≥ 1
 *      CALL_PROCONDUCTOR.
 *
 *   σ_rsi (surface hygiene):
 *       σ_rsi = 1 −
 *         (calibration_rows_ok + calibration_monotone_ok +
 *          calibration_anchor_ok +
 *          arch_rows_ok + arch_chosen_ok + arch_distinct_ok +
 *          threshold_rows_ok + threshold_canonical_ok +
 *          threshold_distinct_ok +
 *          goedel_rows_ok + goedel_both_ok) /
 *         (4 + 1 + 1 + 3 + 1 + 1 + 3 + 1 + 1 + 3 + 1)
 *   v0 requires `σ_rsi == 0.0`.
 *
 *   v278.1 (named, not implemented): live calibration
 *     tuning on a feedback stream, real CMA-ES
 *     architecture search over σ-gate channel counts,
 *     per-domain τ adaptation fed by production σ
 *     histograms, and a production proconductor call-
 *     out pipeline when σ_goedel exceeds τ_goedel.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V278_RSI_H
#define COS_V278_RSI_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V278_N_EPOCHS    4
#define COS_V278_N_ARCH      3
#define COS_V278_N_DOMAINS   3
#define COS_V278_N_GOEDEL    3

typedef enum {
    COS_V278_ACT_SELF_CONFIDENT    = 1,
    COS_V278_ACT_CALL_PROCONDUCTOR = 2
} cos_v278_act_t;

typedef struct {
    int   epoch;
    float sigma_calibration_err;
} cos_v278_calib_t;

typedef struct {
    int   n_channels;
    float aurcc;
    bool  chosen;
} cos_v278_arch_t;

typedef struct {
    char  domain[12];
    float tau;
} cos_v278_thresh_t;

typedef struct {
    int             claim_id;
    float           sigma_goedel;
    cos_v278_act_t  action;
} cos_v278_goedel_t;

typedef struct {
    cos_v278_calib_t   calibration[COS_V278_N_EPOCHS];
    cos_v278_arch_t    arch        [COS_V278_N_ARCH];
    cos_v278_thresh_t  thresholds  [COS_V278_N_DOMAINS];
    cos_v278_goedel_t  goedel      [COS_V278_N_GOEDEL];

    float tau_goedel;   /* 0.40 */

    int   n_calibration_rows_ok;
    bool  calibration_monotone_ok;
    bool  calibration_anchor_ok;

    int   n_arch_rows_ok;
    int   arch_chosen_idx;
    bool  arch_chosen_ok;
    int   n_distinct_aurcc;
    bool  arch_distinct_ok;

    int   n_threshold_rows_ok;
    bool  threshold_canonical_ok;
    int   n_distinct_tau;
    bool  threshold_distinct_ok;

    int   n_goedel_rows_ok;
    int   n_goedel_self;
    int   n_goedel_procon;

    float sigma_rsi;

    bool     chain_valid;
    uint64_t terminal_hash;
    uint64_t seed;
} cos_v278_state_t;

void   cos_v278_init(cos_v278_state_t *s, uint64_t seed);
void   cos_v278_run (cos_v278_state_t *s);

size_t cos_v278_to_json(const cos_v278_state_t *s,
                         char *buf, size_t cap);

int    cos_v278_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V278_RSI_H */
