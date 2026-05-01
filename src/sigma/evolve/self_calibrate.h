/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/*
 * σ-gate self-calibration.  OMEGA-4.
 *
 * Takes a labeled fixture (correct, σ_combined) and searches τ such
 * that a caller-specified loss is minimised.  Two loss modes:
 *
 *   BALANCED:      loss(τ) = α · false_accept(τ) + (1-α) · false_reject(τ)
 *   RISK_BOUNDED:  loss(τ) = false_reject(τ)
 *                 subject to false_accept(τ) ≤ budget_alpha
 *
 * Where:
 *   false_accept(τ) = Pr[ σ ≤ τ ∧ ¬correct ] / Pr[ σ ≤ τ ]
 *   false_reject(τ) = Pr[ σ > τ ∧  correct ] / Pr[ σ > τ ]
 *
 * RISK_BOUNDED is the conformal-style operating point.  We binary-
 * search τ on a 201-point grid (0.00 … 1.00 step 0.005) — this is
 * a direct one-dimensional sweep, no heuristics.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef COS_SIGMA_SELF_CALIBRATE_H
#define COS_SIGMA_SELF_CALIBRATE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float sigma;
    int   correct;
} cos_calib_point_t;

typedef enum {
    COS_CALIB_BALANCED     = 0,
    COS_CALIB_RISK_BOUNDED = 1,
} cos_calib_mode_t;

typedef struct {
    float tau;                /* selected threshold            */
    float false_accept;       /* at the selected τ              */
    float false_reject;
    float accept_rate;        /* coverage                       */
    int   n_points;
    float best_loss;
} cos_calib_result_t;

int  cos_sigma_self_calibrate(const cos_calib_point_t *pts, int n,
                              cos_calib_mode_t mode,
                              float alpha_or_budget,
                              cos_calib_result_t *out);

int  cos_sigma_self_calibrate_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_SIGMA_SELF_CALIBRATE_H */
