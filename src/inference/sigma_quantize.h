/* SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 * Per-layer symmetric integer quant + distortion proxy (libc only).
 */
#ifndef SIGMA_QUANTIZE_MODEL_H
#define SIGMA_QUANTIZE_MODEL_H

#include <stdint.h>

#define COS_SIGMA_QUANT_MAX_LAYERS 128

typedef struct {
    int32_t n_layers;
    int8_t  layer_bits[COS_SIGMA_QUANT_MAX_LAYERS];
    int32_t layer_distortion_q16[COS_SIGMA_QUANT_MAX_LAYERS];
} sigma_quant_config_t;

int32_t sigma_quant_symmetric_max_level(int8_t bits);

void sigma_quant_layer_symmetric(const int32_t *weights_fp, int8_t *weights_q,
                                int32_t n, int8_t bits);

int32_t sigma_quant_layer_distortion_q16(const int32_t *weights_fp,
                                        const int8_t *weights_q, int32_t n,
                                        int8_t bits);

void sigma_quant_auto_layers(sigma_quant_config_t *cfg,
                            const int32_t *const *planes, const int32_t *sizes,
                            int32_t n_planes, int32_t max_distortion_q16);

void sigma_quant_print_summary(const sigma_quant_config_t *cfg);

#endif /* SIGMA_QUANTIZE_MODEL_H */
