/* SPDX-License-Identifier: AGPL-3.0-or-later
 * v40: σ-syndrome → action decoder (lab; not merge-gate policy in production).
 */
#ifndef CREATION_OS_SIGMA_SYNDROME_DECODER_H
#define CREATION_OS_SIGMA_SYNDROME_DECODER_H

typedef enum {
    ACTION_EMIT = 0,
    ACTION_ABSTAIN,
    ACTION_FALLBACK,
    ACTION_RESAMPLE,
    ACTION_DECOMPOSE,
    ACTION_CITE,
} sigma_action_t;

/* Fixed channel slots (indices into sigma[] / threshold[]). */
enum {
    CH_LOGIT_ENTROPY = 0,
    CH_TOP_MARGIN = 1,
    CH_REPETITION = 2,
    CH_EPISTEMIC = 3,
    CH_ALEATORIC = 4,
    CH_HARDWARE = 5,
    CH_AUX0 = 6,
    CH_AUX1 = 7,
    CH_COUNT = 8
};

/**
 * Decode a σ “syndrome” vector against per-channel thresholds.
 * sigma[i] > threshold[i] counts as “high”.
 */
sigma_action_t decode_sigma_syndrome(
    const float *sigma, int n_channels, const float *calibrated_threshold);

#endif /* CREATION_OS_SIGMA_SYNDROME_DECODER_H */
