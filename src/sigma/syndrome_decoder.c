/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "syndrome_decoder.h"

static int has_channel(const int *which, int high_count, int ch)
{
    for (int i = 0; i < high_count; i++) {
        if (which[i] == ch) {
            return 1;
        }
    }
    return 0;
}

sigma_action_t decode_sigma_syndrome(const float *sigma, int n_channels, const float *calibrated_threshold)
{
    int which_high[CH_COUNT];
    int high_count = 0;

    if (!sigma || !calibrated_threshold || n_channels <= 0) {
        return ACTION_ABSTAIN;
    }
    if (n_channels > CH_COUNT) {
        n_channels = CH_COUNT;
    }

    for (int i = 0; i < n_channels; i++) {
        float th = calibrated_threshold[i];
        if (!(th >= 0.0f)) {
            th = 0.0f;
        }
        if (sigma[i] > th && high_count < CH_COUNT) {
            which_high[high_count++] = i;
        }
    }

    if (high_count == 0) {
        return ACTION_EMIT;
    }
    if (high_count == 1 && which_high[0] == CH_REPETITION) {
        return ACTION_RESAMPLE;
    }
    if (high_count == 1 && which_high[0] == CH_TOP_MARGIN) {
        return ACTION_CITE;
    }
    if (high_count >= 4) {
        return ACTION_ABSTAIN;
    }
    if (high_count >= 2 && has_channel(which_high, high_count, CH_EPISTEMIC)) {
        return ACTION_FALLBACK;
    }
    return ACTION_DECOMPOSE;
}
