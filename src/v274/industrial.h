/*
 * v274 σ-Industrial — Industry 4.0 σ-governance.
 *
 *   Factories, processes, supply chain — all gated by
 *   σ.  v274 types four sub-manifests: process σ per
 *   parameter, supply-chain σ per link, quality
 *   prediction σ, and OEE with σ_oee as a
 *   meta-measurement (confidence in OEE itself).
 *
 *   v0 manifests (strict, enumerated):
 *
 *   Process σ (exactly 4 parameters, canonical order):
 *     temperature · pressure · speed · material.
 *     Each: `σ_param ∈ [0, 1]`.
 *     Aggregate: `σ_process = max(σ_param_i)`.
 *     Rule: `σ_process ≤ τ_process = 0.40 →
 *            action = CONTINUE` else → `HALT`.
 *     Contract: `σ_process == max(σ_param)`; action
 *     matches τ_process.  This fixture exercises the
 *     HALT branch — σ_material = 0.55 forces
 *     σ_process = 0.55 > τ_process, action = HALT.
 *
 *   Supply chain σ (exactly 4 links, canonical order):
 *     supplier · factory · distribution · customer.
 *     Each: `σ_link ∈ [0, 1]`,
 *     `backup_activated == (σ_link > τ_backup = 0.45)`.
 *     Contract: ≥ 1 link with backup activated AND
 *     ≥ 1 link without.
 *
 *   Quality prediction (exactly 3 rows):
 *     Each: `batch_id`, `σ_quality ∈ [0, 1]`,
 *     `action ∈ {SKIP_MANUAL, REQUIRE_MANUAL}`,
 *     rule: `σ_quality ≤ τ_quality = 0.25 →
 *            SKIP_MANUAL` else → `REQUIRE_MANUAL`.
 *     Contract: ≥ 1 SKIP_MANUAL AND ≥ 1
 *     REQUIRE_MANUAL.
 *
 *   OEE + σ_oee (exactly 3 shift fixtures):
 *     Each shift: `availability, performance, quality
 *     ∈ [0, 1]`,
 *     `oee = availability * performance * quality`,
 *     `σ_oee ∈ [0, 1]` (confidence in OEE itself),
 *     `trustworthy == (σ_oee ≤ τ_oee = 0.20)`.
 *     Contract: ≥ 1 trustworthy AND ≥ 1
 *     untrustworthy shift; `oee` within 1e-5 of the
 *     product rule.  This is the meta-measurement
 *     contract — when the measurement itself is
 *     uncertain, the OEE headline is marked
 *     untrustworthy.
 *
 *   σ_industrial (surface hygiene):
 *       σ_industrial = 1 −
 *         (process_params_ok + process_agg_ok +
 *          process_action_ok +
 *          supply_rows_ok + supply_both_ok +
 *          quality_rows_ok + quality_both_ok +
 *          oee_rows_ok + oee_formula_ok +
 *          oee_both_ok) /
 *         (4 + 1 + 1 + 4 + 1 + 3 + 1 + 3 + 1 + 1)
 *   v0 requires `σ_industrial == 0.0`.
 *
 *   Contracts (v0):
 *     1. 4 process params canonical; σ_process ==
 *        max(σ_param); action matches τ_process.
 *     2. 4 supply links canonical; backup matches
 *        σ_link vs τ_backup; both branches fire.
 *     3. 3 quality rows; action matches σ_quality vs
 *        τ_quality; both branches fire.
 *     4. 3 OEE shifts; oee == a*p*q (within 1e-5);
 *        trustworthy matches σ_oee vs τ_oee; both
 *        branches fire.
 *     5. σ_industrial ∈ [0, 1] AND == 0.0.
 *     6. FNV-1a chain replays byte-identically.
 *
 *   v274.1 (named, not implemented): live MES / SCADA
 *     integration, OPC-UA transport, measured
 *     σ_process from a real line, supply-chain σ
 *     reacting to live ERP data, OEE σ wired to v181
 *     audit so every meta-measurement is receipt-
 *     stamped.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V274_INDUSTRIAL_H
#define COS_V274_INDUSTRIAL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V274_N_PROCESS  4
#define COS_V274_N_SUPPLY   4
#define COS_V274_N_QUALITY  3
#define COS_V274_N_OEE      3

typedef enum {
    COS_V274_PROC_CONTINUE = 1,
    COS_V274_PROC_HALT     = 2
} cos_v274_proc_t;

typedef enum {
    COS_V274_Q_SKIP_MANUAL    = 1,
    COS_V274_Q_REQUIRE_MANUAL = 2
} cos_v274_q_t;

typedef struct {
    char   param[16];
    float  sigma_param;
} cos_v274_param_t;

typedef struct {
    char   link[16];
    float  sigma_link;
    bool   backup_activated;
} cos_v274_supply_t;

typedef struct {
    char          batch_id[12];
    float         sigma_quality;
    cos_v274_q_t  action;
} cos_v274_quality_t;

typedef struct {
    char   shift[12];
    float  availability;
    float  performance;
    float  quality;
    float  oee;
    float  sigma_oee;
    bool   trustworthy;
} cos_v274_oee_t;

typedef struct {
    cos_v274_param_t    params [COS_V274_N_PROCESS];
    cos_v274_supply_t   supply [COS_V274_N_SUPPLY];
    cos_v274_quality_t  quality[COS_V274_N_QUALITY];
    cos_v274_oee_t      oee    [COS_V274_N_OEE];

    float tau_process;   /* 0.40 */
    float tau_backup;    /* 0.45 */
    float tau_quality;   /* 0.25 */
    float tau_oee;       /* 0.20 */

    float sigma_process;
    cos_v274_proc_t process_action;
    bool  process_aggregation_ok;
    bool  process_action_ok;

    int   n_process_params_ok;
    int   n_supply_rows_ok;
    int   n_supply_backup;
    int   n_supply_ok_link;
    int   n_quality_rows_ok;
    int   n_quality_skip;
    int   n_quality_require;
    int   n_oee_rows_ok;
    int   n_oee_formula_ok;
    int   n_oee_trust;
    int   n_oee_untrust;

    float sigma_industrial;

    bool     chain_valid;
    uint64_t terminal_hash;
    uint64_t seed;
} cos_v274_state_t;

void   cos_v274_init(cos_v274_state_t *s, uint64_t seed);
void   cos_v274_run (cos_v274_state_t *s);

size_t cos_v274_to_json(const cos_v274_state_t *s,
                         char *buf, size_t cap);

int    cos_v274_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V274_INDUSTRIAL_H */
