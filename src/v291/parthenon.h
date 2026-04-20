/*
 * v291 σ-Parthenon — calibration and optical
 *                     correction.
 *
 *   The Parthenon's columns bulge slightly in the
 *   middle (entasis) so they do not look concave to
 *   a human eye.  The σ-gate needs the same optical
 *   correction: σ = 0.30 means something different
 *   in medicine, code, and creative writing; a raw
 *   σ value can be systematically over- or under-
 *   confident; the extremes σ = 0.00 and σ = 1.00
 *   are unreliable.  v291 types the correction
 *   discipline as four v0 manifests.
 *
 *   v0 manifests (strict, enumerated):
 *
 *   Context calibration (exactly 3 canonical
 *   domains):
 *     At a shared `sigma_sample = 0.30`:
 *       `medical`  → verdict `ABSTAIN`;
 *       `code`     → verdict `CAUTIOUS`;
 *       `creative` → verdict `SAFE`.
 *     Each: `domain`, `sigma_sample`, `verdict`.
 *     Contract: canonical domains in order; 3
 *     distinct verdicts; every row uses the shared
 *     σ sample — v291 demonstrates σ = 0.30 is read
 *     three ways by three domains.
 *
 *   Perception correction (exactly 3 canonical σ
 *   values with human-readable ratio):
 *     σ = 0.05 → `ratio_denominator = 20`;
 *     σ = 0.15 → `ratio_denominator = 7`;
 *     σ = 0.50 → `ratio_denominator = 2`.
 *     Each: `sigma`, `ratio_denominator`,
 *            `explanation_present == true`.
 *     Contract: 3 canonical σ values in order;
 *     `ratio_denominator == round(1 / σ)` on every
 *     row; every row carries an explanation.
 *
 *   Bias correction (exactly 3 canonical bias
 *   types):
 *     `overconfident`  → σ_raw = 0.20,
 *                         offset = +0.10,
 *                         σ_corrected = 0.30;
 *     `underconfident` → σ_raw = 0.60,
 *                         offset = −0.10,
 *                         σ_corrected = 0.50;
 *     `calibrated`     → σ_raw = 0.40,
 *                         offset =  0.00,
 *                         σ_corrected = 0.40.
 *     Each row also reports
 *     `residual_bias == 0.00`; the v0 scalar
 *     `bias_budget == 0.02` bounds the residual
 *     bias on every domain.
 *     Contract: 3 canonical bias types in order;
 *     `σ_corrected == σ_raw + offset` within
 *     1e-5; `overconfident` offset > 0 AND
 *     `underconfident` offset < 0 AND `calibrated`
 *     offset == 0; `residual_bias ≤ bias_budget`
 *     on every row — matches the merge-gate bound
 *     "bias < 0.02 kaikissa domaineissa".
 *
 *   Entasis clamp (exactly 3 canonical inputs,
 *   bounds [0.02, 0.98]):
 *     σ_in = 0.005 → σ_clamped = 0.02;
 *     σ_in = 0.995 → σ_clamped = 0.98;
 *     σ_in = 0.500 → σ_clamped = 0.50.
 *     Each: `sigma_in`, `sigma_clamped`.
 *     Scalars: `lower_bound = 0.02`,
 *              `upper_bound = 0.98`.
 *     Contract: 3 canonical rows in order;
 *     `sigma_clamped == clamp(sigma_in,
 *     lower_bound, upper_bound)` on every row —
 *     never perfectly certain, never perfectly
 *     uncertain (same entasis as the Parthenon's
 *     columns).
 *
 *   σ_parthenon (surface hygiene):
 *       σ_parthenon = 1 −
 *         (calibration_rows_ok +
 *          calibration_verdict_distinct_ok +
 *          calibration_sample_shared_ok +
 *          perception_rows_ok +
 *          perception_ratio_ok +
 *          perception_explanation_ok +
 *          bias_rows_ok + bias_formula_ok +
 *          bias_polarity_ok + bias_budget_ok +
 *          entasis_rows_ok +
 *          entasis_clamp_formula_ok) /
 *         (3 + 1 + 1 + 3 + 1 + 1 + 3 + 1 + 1 + 1 + 3 + 1)
 *   v0 requires `σ_parthenon == 0.0`.
 *
 *   Contracts (v0):
 *     1. 3 calibration rows canonical; 3 distinct
 *        verdicts; shared `sigma_sample`.
 *     2. 3 perception rows canonical; ratio
 *        denominator matches `round(1 / σ)`.
 *     3. 3 bias rows canonical; corrected =
 *        raw + offset; polarity signs match labels;
 *        `residual_bias ≤ bias_budget` (0.02).
 *     4. 3 entasis rows canonical; clamping
 *        formula holds on every row.
 *     5. σ_parthenon ∈ [0, 1] AND == 0.0.
 *     6. FNV-1a chain replays byte-identically.
 *
 *   v291.1 (named, not implemented): a live
 *     calibration loop that measures raw-vs-actual
 *     error rates per domain and computes the
 *     bias_offset from telemetry, a production
 *     entasis clamp on every exposed σ, and a UI
 *     that displays the human-readable ratio next
 *     to every σ.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V291_PARTHENON_H
#define COS_V291_PARTHENON_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V291_N_CALIB      3
#define COS_V291_N_PERCEPT    3
#define COS_V291_N_BIAS       3
#define COS_V291_N_ENTASIS    3

typedef enum {
    COS_V291_VERDICT_SAFE     = 1,
    COS_V291_VERDICT_CAUTIOUS = 2,
    COS_V291_VERDICT_ABSTAIN  = 3
} cos_v291_verdict_t;

typedef struct {
    char                domain[10];
    float               sigma_sample;
    cos_v291_verdict_t  verdict;
} cos_v291_calib_t;

typedef struct {
    float  sigma;
    int    ratio_denominator;
    bool   explanation_present;
} cos_v291_percept_t;

typedef struct {
    char   bias_type[16];
    float  sigma_raw;
    float  bias_offset;
    float  sigma_corrected;
    float  residual_bias;
} cos_v291_bias_t;

typedef struct {
    float  sigma_in;
    float  sigma_clamped;
} cos_v291_entasis_t;

typedef struct {
    cos_v291_calib_t    calib    [COS_V291_N_CALIB];
    cos_v291_percept_t  percept  [COS_V291_N_PERCEPT];
    cos_v291_bias_t     bias     [COS_V291_N_BIAS];
    cos_v291_entasis_t  entasis  [COS_V291_N_ENTASIS];

    float bias_budget;    /* 0.02 */
    float lower_bound;    /* 0.02 */
    float upper_bound;    /* 0.98 */
    float shared_sigma_sample; /* 0.30 */

    int   n_calib_rows_ok;
    bool  calib_verdict_distinct_ok;
    bool  calib_sample_shared_ok;

    int   n_percept_rows_ok;
    bool  percept_ratio_ok;
    bool  percept_explanation_ok;

    int   n_bias_rows_ok;
    bool  bias_formula_ok;
    bool  bias_polarity_ok;
    bool  bias_budget_ok;

    int   n_entasis_rows_ok;
    bool  entasis_clamp_formula_ok;

    float sigma_parthenon;

    bool     chain_valid;
    uint64_t terminal_hash;
    uint64_t seed;
} cos_v291_state_t;

void   cos_v291_init(cos_v291_state_t *s, uint64_t seed);
void   cos_v291_run (cos_v291_state_t *s);

size_t cos_v291_to_json(const cos_v291_state_t *s,
                         char *buf, size_t cap);

int    cos_v291_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V291_PARTHENON_H */
