/* SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 * Integer-only single-head attention (fixed-point softmax approximation).
 */
#ifndef COS_INTEGER_ATTENTION_H
#define COS_INTEGER_ATTENTION_H

#include <stdint.h>

void integer_attention(const int32_t *Q, const int32_t *K, const int32_t *V,
                      int32_t *out, int32_t seq_len, int32_t head_dim);

#endif /* COS_INTEGER_ATTENTION_H */
