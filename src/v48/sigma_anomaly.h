/* SPDX-License-Identifier: AGPL-3.0-or-later
 * v48 lab: σ-pattern anomaly heuristics (logit-derived per-token σ only — not prompt text).
 */
#ifndef CREATION_OS_V48_SIGMA_ANOMALY_H
#define CREATION_OS_V48_SIGMA_ANOMALY_H

#include "../sigma/decompose.h"

typedef struct {
    float sigma_mean;
    float sigma_variance;
    float sigma_trend;
    float sigma_spike_count;
    float instruction_sigma;
    float response_sigma;
    float gap;
    int anomaly_detected;
} sigma_anomaly_t;

/**
 * @param per_token_sigmas epistemic/aleatoric/total per token (typically from Dirichlet σ).
 * @param baseline_sigma_profile optional per-position epistemic profile from clean runs (may be NULL).
 */
sigma_anomaly_t detect_anomaly(
    const sigma_decomposed_t *per_token_sigmas,
    int n_tokens,
    const float *baseline_sigma_profile,
    int baseline_len);

#endif /* CREATION_OS_V48_SIGMA_ANOMALY_H */
