/* SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 * Per-row KV packing: full int32 planes in sigma_kv_cache, optional packed
 * side buffers for byte accounting and future decode path.
 */
#ifndef SIGMA_KV_QUANTIZE_H
#define SIGMA_KV_QUANTIZE_H

#include <stdint.h>

typedef enum {
    COS_KV_PREC_I32 = 0,
    COS_KV_PREC_Q4  = 1,
    COS_KV_PREC_Q2  = 2,
} cos_kv_precision_t;

int32_t cos_kv_q4_packed_bytes(int32_t head_dim);
int32_t cos_kv_q2_packed_bytes(int32_t head_dim);

void cos_kv_pack_row_q4(const int32_t *src, int32_t head_dim, uint8_t *dst,
                       int32_t dst_cap);
void cos_kv_unpack_row_q4(const uint8_t *src, int32_t head_dim, int32_t *dst);

void cos_kv_pack_row_q2(const int32_t *src, int32_t head_dim, uint8_t *dst,
                       int32_t dst_cap);
void cos_kv_unpack_row_q2(const uint8_t *src, int32_t head_dim, int32_t *dst);

/* Choose precision from σ thresholds (Q16); mid/high compress K and V rows. */
cos_kv_precision_t cos_kv_precision_for_sigma(int32_t sigma_q16);

#endif /* SIGMA_KV_QUANTIZE_H */
