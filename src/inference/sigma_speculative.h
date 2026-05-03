/* SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 * σ-speculative decoding: draft proposal + σ mirror, optional target verify.
 *
 * KV is only consistent when draft and target share the same tensor geometry
 * (same model pointer or identical layout). Otherwise target verification is
 * always used (no σ-skip).
 */
#ifndef SIGMA_SPECULATIVE_H
#define SIGMA_SPECULATIVE_H

#include "cos_sigma_mirror.h"
#include "token_pipeline.h"
#include <stdint.h>

typedef struct {
    inference_state_t *draft;
    inference_state_t *target;
    cos_sigma_q16_state_t gate;
    int32_t              draft_len;
    int32_t              sigma_skip_tau_q16;
    uint32_t             target_skipped;
    uint32_t             target_called;
    int32_t              same_geometry;
} sigma_speculative_t;

void sigma_speculative_init(sigma_speculative_t *s, inference_state_t *draft,
                           inference_state_t *target, int32_t draft_len,
                           int32_t sigma_skip_tau_q16);

int32_t sigma_speculative_step(sigma_speculative_t *spec, int32_t cur_token,
                              int32_t *draft_tokens_out, int32_t draft_cap,
                              int32_t *accepted, int32_t accepted_cap);

int32_t sigma_adaptive_draft_len_q16(const sigma_speculative_t *spec,
                                    int32_t base_len);

#endif /* SIGMA_SPECULATIVE_H */
