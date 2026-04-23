/*
 * Runtime consciousness index — operational σ-integration metrics (not IIT-Φ).
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef COS_SIGMA_CONSCIOUSNESS_METER_H
#define COS_SIGMA_CONSCIOUSNESS_METER_H

#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_CONSCIOUSNESS_N_CHANNELS 4

struct cos_consciousness_state {
    float    phi_proxy;
    float    integration;
    float    differentiation;
    float    self_model_accuracy;

    float    broadcast_strength;
    float    access_latency_ms;

    float    meta_awareness;
    float    loop_detection;
    float    temporal_continuity;

    float    consciousness_index;
    int      level;

    int64_t  timestamp_ms;
};

int cos_consciousness_init(struct cos_consciousness_state *cs);

int cos_consciousness_measure(struct cos_consciousness_state *cs,
                              float                         sigma_combined,
                              const float                  *sigma_channels,
                              const float                  *meta_channels,
                              float                         prev_sigma);

int cos_consciousness_classify_level(
    const struct cos_consciousness_state *cs);

char *cos_consciousness_to_json(
    const struct cos_consciousness_state *cs);

void cos_consciousness_print(const struct cos_consciousness_state *cs,
                             FILE                               *fp);

int cos_consciousness_self_test(void);

#ifdef __cplusplus
}
#endif
#endif /* COS_SIGMA_CONSCIOUSNESS_METER_H */
