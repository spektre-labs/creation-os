/* SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 * One-token forward + σ mirror update (libc only, fixed buffers).
 */
#ifndef COS_TOKEN_PIPELINE_H
#define COS_TOKEN_PIPELINE_H

#include "cos_sigma_mirror.h"
#include "ternary_engine.h"
#include <stdint.h>

typedef struct {
    ternary_model_t model;
    cos_sigma_q16_state_t gate;
    int32_t *kv_cache_k;
    int32_t *kv_cache_v;
    int32_t cache_len;
    int32_t max_cache;
} inference_state_t;

void inference_state_init(inference_state_t *st, ternary_model_t *model,
                         int32_t *kv_k, int32_t *kv_v, int32_t max_cache);

int32_t generate_token(inference_state_t *state, int32_t input_token);

#endif /* COS_TOKEN_PIPELINE_H */
