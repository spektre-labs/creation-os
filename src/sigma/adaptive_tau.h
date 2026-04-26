/*
 * Runtime τ_accept from graded conformal CSV (optional production hook).
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef COS_SIGMA_ADAPTIVE_TAU_H
#define COS_SIGMA_ADAPTIVE_TAU_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Resolve default CSV path: COS_ADAPTIVE_TAU_CSV, then
 * $CREATION_OS_ROOT/benchmarks/graded/conformal_thresholds.csv, then
 * benchmarks/graded/conformal_thresholds.csv (cwd).
 * Returns 0 on success.
 *
 * Temperature T from benchmarks/graded/temperature.txt (see
 * cos_adaptive_graded_temperature) can optionally nudge τ when
 * COS_ADAPTIVE_TAU_TEMPERATURE_BLEND=1.
 */
int cos_adaptive_tau_default_csv_path(char *buf, size_t cap);

/** Default path for graded temperature.txt (same discovery as CSV). */
int cos_adaptive_tau_default_temperature_path(char *buf, size_t cap);

/** Cached optimal T from graded run; 1.0f if file missing or invalid. */
float cos_adaptive_graded_temperature(void);

/**
 * Read τ for target_accuracy ≈ 0.90 (α ≈ 0.10) from a conformal_thresholds.csv
 * row. On failure returns fallback_accept (typically 0.3f).
 * Negative cache sentinel cleared by cos_adaptive_tau_invalidate_cache().
 */
float cos_adaptive_tau_accept_from_csv(const char *csv_path,
                                       float fallback_accept);

/** Same as cos_adaptive_tau_accept_from_csv(NULL, 0.3f). */
float cos_adaptive_tau_accept(void);

/** τ_rethink = accept + (1 - accept) * 0.5 (midpoint toward certainty). */
float cos_adaptive_tau_rethink_from_accept(float tau_accept);

/** Rethink τ from cos_adaptive_tau_accept() (uses CSV cache). */
float cos_adaptive_tau_rethink(void);

void cos_adaptive_tau_invalidate_cache(void);

#ifdef __cplusplus
}
#endif
#endif /* COS_SIGMA_ADAPTIVE_TAU_H */
