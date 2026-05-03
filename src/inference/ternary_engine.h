/* SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 * Ternary {-1,0,+1} GEMV — scalar, packed 2-bit, ARM NEON, optional x86 AVX2, RISC-V RVV.
 * Packed layout matches v158 σ‑BitNet lab: 4 weights / byte (00=0, 01=+1, 10=−1, 11 reserved→0).
 * σ-gate numerics: ``cos_sigma_mirror.h`` (does **not** modify ``python/cos/sigma_gate.h``).
 */
#ifndef COS_TERNARY_ENGINE_H
#define COS_TERNARY_ENGINE_H

#include "cos_inference_limits.h"
#include "cos_sigma_mirror.h"
#include <stdint.h>

typedef struct {
    int8_t *weights; /* row-major, values in {-1,0,+1} */
    int32_t *biases; /* optional row bias in Q16.16; may be NULL */
    int32_t rows;
    int32_t cols;
} ternary_layer_t;

typedef struct {
    uint8_t *packed_weights; /* ceil(rows*cols/4) bytes, 2 bits / weight */
    int32_t  scale_q16;     /* dequant: (acc * scale_q16) >> 16 */
    int32_t  rows;
    int32_t  cols;
} cos_infer_ternary_layer_packed_t;

typedef struct {
    int8_t *embed; /* vocab * hidden int8 activations / embeddings */
    ternary_layer_t *layers; /* per-block FFN; count = n_layer_blocks */
    int32_t n_layer_blocks;
    ternary_layer_t lm_head; /* vocab x hidden */
    int32_t vocab_size;
    int32_t hidden_dim;
    int32_t n_heads;
    int32_t head_dim;
} ternary_model_t;

void ternary_matmul(const int8_t *W, const int8_t *x, int32_t *out, int32_t M,
                    int32_t N);

/** Pack ``n_weights`` ternary values from ``w`` (each −1,0,+1) into ``packed`` (caller size ≥ (n+3)/4). */
void cos_infer_ternary_pack_int8(const int8_t *w, uint8_t *packed, int32_t n_weights);

void ternary_matmul_packed(const uint8_t *packed, const int32_t *x, int32_t *out, int32_t M,
                           int32_t N, int32_t scale_q16);

float cos_infer_ternary_sparsity_packed(const uint8_t *packed, int32_t total_weights);

/** Map activation magnitude to σ in Q16 (higher |x| ⇒ higher σ — lab heuristic). */
int32_t cos_infer_ternary_layer_sigma_q16(const int32_t *act, int32_t len);

/**
 * Layered packed forward with σ-gate after each layer.
 * Returns ``n_layers`` if all layers ran; else 0..n_layers−1 = index of layer after which ABSTAIN fired.
 * If ``confident_exit_sigma_q16`` > 0 and computed layer σ is **below** it, returns ``-(L+1)`` (1-based negated; L = layer index).
 */
int32_t cos_infer_ternary_stack_forward(
    const cos_infer_ternary_layer_packed_t *layers, int32_t n_layers, int32_t *workspace,
    int32_t workspace_words, const int32_t *input, int32_t *out_final, int32_t *out_rows,
    cos_sigma_q16_state_t *gate, int32_t k_raw_q16, int32_t confident_exit_sigma_q16);

#endif /* COS_TERNARY_ENGINE_H */
