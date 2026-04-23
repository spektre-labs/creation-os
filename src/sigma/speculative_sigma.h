/*
 * Speculative σ — predict uncertainty before full inference (edge layer).
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef COS_SIGMA_SPECULATIVE_SIGMA_H
#define COS_SIGMA_SPECULATIVE_SIGMA_H

#include <stdint.h>

struct cos_speculative_sigma {
    float predicted_sigma;
    float confidence;
    int   skip_verification; /* 1 → cheap path: skip SCI-5 shadow regen */
    int   early_abstain;     /* 1 → abstain without generation */
};

struct cos_speculative_sigma cos_predict_sigma(const uint64_t *bsc_prompt,
                                               uint64_t domain_hash);

void cos_speculative_sigma_observe(uint64_t domain_hash, float measured_sigma);

int cos_speculative_sigma_self_test(void);

#endif /* COS_SIGMA_SPECULATIVE_SIGMA_H */
