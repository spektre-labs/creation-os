/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "adaptive_quant.h"

#include <stddef.h>

v46_quant_config_t v46_sigma_adaptive_quant(const sigma_decomposed_t *s)
{
    v46_quant_config_t q;
    q.activation_bits = 8;
    q.sparsity = 0.0f;
    if (!s) {
        return q;
    }
    if (s->epistemic < 0.2f) {
        q.activation_bits = 4;
        q.sparsity = 0.55f;
    } else if (s->epistemic < 0.5f) {
        q.activation_bits = 8;
        q.sparsity = 0.3f;
    } else {
        q.activation_bits = 8;
        q.sparsity = 0.0f;
    }
    return q;
}
