/* SPDX-License-Identifier: AGPL-3.0-or-later
 * v45 lab: σ as introspection — calibration gap between verbalized confidence and σ-derived confidence.
 *
 * Evidence class: **lab (C)** — deterministic stand-in for “self-report + resample” until a hosted LM loop exists.
 */
#ifndef CREATION_OS_V45_INTROSPECTION_H
#define CREATION_OS_V45_INTROSPECTION_H

typedef struct {
    float confidence_self_report;
    float confidence_actual;
    float calibration_gap;
    float introspective_accuracy;
    float meta_sigma;
} v45_introspection_state_t;

/**
 * `self_report_in` may be NaN → synthesize a stable self-report from (prompt, response) hashes.
 * `introspective_accuracy` is a toy scalar: max(0, 1 − calibration_gap) for lab dashboards only.
 */
void v45_measure_introspection_lab(const char *prompt, const char *response, float self_report_in,
    v45_introspection_state_t *out);

#endif /* CREATION_OS_V45_INTROSPECTION_H */
