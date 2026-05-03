/* SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 * Minimal GGUF header probe — full tensor parse is future work.
 * Named distinctly from `src/import/gguf_loader.h` (include-path safe).
 */
#ifndef COS_INFERENCE_GGUF_H
#define COS_INFERENCE_GGUF_H

#include "ternary_engine.h"
#include <stdint.h>

typedef struct {
    int8_t embed[COS_INF_MAX_VOCAB * COS_INF_MAX_HIDDEN];
    int8_t ffn_w[COS_INF_MAX_LAYERS][COS_INF_MAX_HIDDEN * COS_INF_MAX_HIDDEN];
    int32_t ffn_bias[COS_INF_MAX_LAYERS][COS_INF_MAX_HIDDEN];
    int8_t lm_w[COS_INF_MAX_VOCAB * COS_INF_MAX_HIDDEN];
    int32_t lm_bias[COS_INF_MAX_VOCAB];
    ternary_layer_t blocks[COS_INF_MAX_LAYERS];
    ternary_layer_t lm_head;
    ternary_model_t model;
} cos_inference_model_storage_t;

void cos_inference_init_toy_model(cos_inference_model_storage_t *buf,
                                  int32_t vocab_size, int32_t hidden_dim,
                                  int32_t head_dim, int32_t n_blocks);

/* Non-zero return: I/O error, bad magic, or dimensions over COS_INF_MAX_*. */
int cos_gguf_load_model(const char *path, ternary_model_t *model);

#endif /* COS_INFERENCE_GGUF_H */
