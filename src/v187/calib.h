/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/*
 * v187 σ-Calibration — continuous σ calibration.
 *
 *   σ is only as useful as its calibration.  If σ=0.3 does not
 *   mean "≈30 % chance of error" then σ is noise.  v187
 *   enforces calibration end-to-end:
 *
 *     1. a holdout of 500 Q&A pairs with known truth is
 *        replayed through the σ-gate stack every week;
 *     2. predictions are bucketed into 10 σ-bins, per-bin
 *        accuracy is measured, Expected Calibration Error is
 *        computed;
 *     3. if ECE > 0.05 a temperature `T` is fit on the holdout
 *        so σ_calibrated = σ^(1/T) minimises ECE;
 *     4. a per-domain T is cached (math / code / history /
 *        general) so each domain keeps its own calibration.
 *
 *   The exit invariants (exercised in the merge-gate):
 *
 *     1. The raw (uncalibrated) fixture has ECE ≥ 0.10 so the
 *        kernel has something to fix.
 *     2. After temperature scaling the global ECE drops below
 *        0.05.
 *     3. Per-domain Ts are all in (0, +∞) and at least one
 *        differs from the global T by more than 0.01.
 *     4. Re-binning the post-calibration predictions yields
 *        strictly lower ECE than the raw ECE on ≥ 4 of the
 *        10 bins.
 *     5. Output byte-deterministic.
 *
 * v187.0 (this file) ships a deterministic, weights-free
 * fixture: 500 synthetic σ/label pairs with injected mis-
 * calibration (raw σ is overconfident).  Temperature search
 * is a closed-form golden-section over T ∈ [0.5, 3.0].
 *
 * v187.1 (named, not shipped):
 *   - real holdout ingestion from the rotation worker
 *   - Platt / isotonic / Beta alternatives behind a flag
 *   - recalibration trigger wired into v159 self-healing and
 *     v181 audit
 *   - Web-UI calibration-curve view per domain
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V187_CALIB_H
#define COS_V187_CALIB_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V187_N_HOLDOUT    500
#define COS_V187_N_BINS        10
#define COS_V187_N_DOMAINS      4
#define COS_V187_STR_MAX       32

typedef struct {
    int   id;
    int   domain;                           /* 0..N_DOMAINS-1 */
    float sigma_raw;                        /* uncalibrated σ */
    float sigma_calibrated;                 /* post-T */
    bool  correct;                          /* ground truth */
} cos_v187_sample_t;

typedef struct {
    int   id;
    float lo;
    float hi;
    int   n;
    int   n_correct;
    float mean_sigma;
    float expected_err_rate;                /* = 1 - mean_sigma */
    float observed_err_rate;                /* = 1 - accuracy */
} cos_v187_bin_t;

typedef struct {
    int     domain;
    char    name[COS_V187_STR_MAX];
    float   temperature;
    float   ece_raw;
    float   ece_calibrated;
} cos_v187_domain_t;

typedef struct {
    int                n_samples;
    cos_v187_sample_t  samples[COS_V187_N_HOLDOUT];

    cos_v187_bin_t     bins_raw [COS_V187_N_BINS];
    cos_v187_bin_t     bins_cal [COS_V187_N_BINS];

    int                n_domains;
    cos_v187_domain_t  domains[COS_V187_N_DOMAINS];

    float              ece_raw;
    float              ece_calibrated;
    float              global_temperature;
    float              tau_ece;             /* alarm threshold */
    uint64_t           seed;

    int                n_bins_improved;
} cos_v187_state_t;

void   cos_v187_init       (cos_v187_state_t *s, uint64_t seed);
void   cos_v187_build      (cos_v187_state_t *s);
void   cos_v187_fit_global (cos_v187_state_t *s);
void   cos_v187_fit_domains(cos_v187_state_t *s);
void   cos_v187_rebin      (cos_v187_state_t *s);

size_t cos_v187_to_json    (const cos_v187_state_t *s,
                             char *buf, size_t cap);

int    cos_v187_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V187_CALIB_H */
