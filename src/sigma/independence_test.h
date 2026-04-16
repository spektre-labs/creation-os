/* SPDX-License-Identifier: AGPL-3.0-or-later
 * v40: σ-channel independence diagnostics (lab).
 */
#ifndef CREATION_OS_SIGMA_INDEPENDENCE_TEST_H
#define CREATION_OS_SIGMA_INDEPENDENCE_TEST_H

#define SIGMA_INDEP_MAX_CHANNELS 8

typedef struct {
    float correlation_matrix[SIGMA_INDEP_MAX_CHANNELS][SIGMA_INDEP_MAX_CHANNELS];
    float mean_correlation;
    int n_independent;
    int threshold_regime; /* 1 if n_independent >= 4 */
} sigma_independence_t;

/**
 * Row-major samples: samples[c * n_samples + s] is channel c at time/sample index s.
 * n_channels must be <= SIGMA_INDEP_MAX_CHANNELS.
 */
sigma_independence_t measure_channel_independence(
    const float *channel_samples, int n_channels, int n_samples);

#endif /* CREATION_OS_SIGMA_INDEPENDENCE_TEST_H */
