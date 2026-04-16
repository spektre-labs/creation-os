/* SPDX-License-Identifier: AGPL-3.0-or-later
 * v46 lab: σ-guided adaptive quantization policy (toy; not wired into a runtime quantizer).
 */
#ifndef CREATION_OS_V46_ADAPTIVE_QUANT_H
#define CREATION_OS_V46_ADAPTIVE_QUANT_H

#include "../sigma/decompose.h"

typedef struct {
    int activation_bits;
    float sparsity;
} v46_quant_config_t;

v46_quant_config_t v46_sigma_adaptive_quant(const sigma_decomposed_t *s);

#endif /* CREATION_OS_V46_ADAPTIVE_QUANT_H */
