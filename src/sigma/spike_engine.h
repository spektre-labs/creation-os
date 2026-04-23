/*
 * Sigma-spike engine — event-driven sigma monitoring (neuromorphic-style gating).
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef COS_SIGMA_SPIKE_ENGINE_H
#define COS_SIGMA_SPIKE_ENGINE_H

#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_SPIKE_CHANNELS 8

struct cos_spike_channel {
    float    value;
    float    threshold;
    float    last_fired_value;
    int64_t  last_fired_ms;
    int      fire_count;
    int      suppress_count;
};

struct cos_spike_engine {
    struct cos_spike_channel channels[COS_SPIKE_CHANNELS];
    int    total_fires;
    int    total_suppressed;
    float  energy_saved_ratio;
    int    primed;
};

int cos_spike_engine_init(struct cos_spike_engine *e);

/**
 * Compare new_values[8] to stored channel values.
 * On first call after init, primes channels (no suppression accounting).
 * Writes fired_channels[i] = 1 when |delta sigma| exceeds channels[i].threshold.
 */
int cos_spike_check(struct cos_spike_engine *e, const float *new_values,
                    int *fired_channels, int *n_fired);

void cos_spike_engine_stats(const struct cos_spike_engine *e,
                            float *energy_saved, int *total_fires,
                            int *total_suppressed);

void cos_spike_engine_print(FILE *fp, const struct cos_spike_engine *e);

/** Persist snapshot for `cos spike stats` (separate process). */
int cos_spike_engine_snapshot_write(const struct cos_spike_engine *e,
                                    const char *path);

int cos_spike_engine_snapshot_read(struct cos_spike_engine *e,
                                   const char *path);

void cos_spike_fill_channels_from_speculative(float predicted_sigma,
                                              float confidence,
                                              float *out8);

int cos_spike_engine_self_test(void);

#ifdef __cplusplus
}
#endif
#endif /* COS_SIGMA_SPIKE_ENGINE_H */
