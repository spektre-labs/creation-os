/* SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only */
#include "cos_sigma_mirror.h"
#include "sigma_kv_quantize.h"
#include <stddef.h>

static int32_t clamp16(int64_t x)
{
    if (x < -32768)
        return -32768;
    if (x > 32767)
        return 32767;
    return (int32_t)x;
}

int32_t cos_kv_q4_packed_bytes(int32_t head_dim)
{
    if (head_dim <= 0)
        return 0;
    return (head_dim + 1) / 2;
}

int32_t cos_kv_q2_packed_bytes(int32_t head_dim)
{
    if (head_dim <= 0)
        return 0;
    return (head_dim + 3) / 4;
}

void cos_kv_pack_row_q4(const int32_t *src, int32_t head_dim, uint8_t *dst,
                       int32_t dst_cap)
{
    int32_t i;
    int32_t need;
    if (!src || !dst || head_dim <= 0)
        return;
    need = cos_kv_q4_packed_bytes(head_dim);
    if (need > dst_cap)
        return;
    for (i = 0; i < head_dim; i += 2) {
        int32_t a = clamp16((int64_t)src[i] >> 12);
        int32_t b = (i + 1 < head_dim) ? clamp16((int64_t)src[i + 1] >> 12) : 0;
        int32_t qa = (a + 8) >> 4; /* map to nibble */
        int32_t qb = (b + 8) >> 4;
        if (qa < 0)
            qa = 0;
        if (qa > 15)
            qa = 15;
        if (qb < 0)
            qb = 0;
        if (qb > 15)
            qb = 15;
        dst[i / 2] = (uint8_t)((qa & 0x0f) | ((qb & 0x0f) << 4));
    }
}

void cos_kv_unpack_row_q4(const uint8_t *src, int32_t head_dim, int32_t *dst)
{
    int32_t i;
    if (!src || !dst || head_dim <= 0)
        return;
    for (i = 0; i < head_dim; i += 2) {
        uint8_t b = src[i / 2];
        dst[i] = (int32_t)(b & 0x0f) << 12;
        if (i + 1 < head_dim)
            dst[i + 1] = (int32_t)((b >> 4) & 0x0f) << 12;
    }
}

void cos_kv_pack_row_q2(const int32_t *src, int32_t head_dim, uint8_t *dst,
                       int32_t dst_cap)
{
    int32_t i;
    int32_t need;
    int32_t shift;
    if (!src || !dst || head_dim <= 0)
        return;
    need = cos_kv_q2_packed_bytes(head_dim);
    if (need > dst_cap)
        return;
    for (i = 0; i < need; i++)
        dst[i] = 0;
    for (i = 0; i < head_dim; i++) {
        int32_t v = clamp16((int64_t)src[i] >> 14);
        int32_t q = (v + 8192) >> 13;
        if (q < 0)
            q = 0;
        if (q > 3)
            q = 3;
        shift = (i & 3) * 2;
        dst[i / 4] |= (uint8_t)((q & 3) << shift);
    }
}

void cos_kv_unpack_row_q2(const uint8_t *src, int32_t head_dim, int32_t *dst)
{
    int32_t i;
    if (!src || !dst || head_dim <= 0)
        return;
    for (i = 0; i < head_dim; i++) {
        int32_t shift = (i & 3) * 2;
        int32_t q = (src[i / 4] >> shift) & 3;
        dst[i] = q << 14;
    }
}

cos_kv_precision_t cos_kv_precision_for_sigma(int32_t sigma_q16)
{
    /* 0.7 * 65536 = 45875; 0.3 * 65536 = 19661 */
    if (sigma_q16 > (int32_t)((7 * COS_SIGMA_Q16_ONE + 5) / 10))
        return COS_KV_PREC_Q2;
    if (sigma_q16 > (int32_t)((3 * COS_SIGMA_Q16_ONE + 5) / 10))
        return COS_KV_PREC_Q4;
    return COS_KV_PREC_I32;
}
