/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Hardware-accelerated Hamming weight for 4096-bit hypervectors (COS_W lanes).
 * AArch64: NEON vcnt + widen-add (two uint64 lanes per step).
 * Other arches: falls back to scalar __builtin_popcountll (same semantics as cos_bsc.h).
 */
#ifndef COS_NEON_HAMMING_H
#define COS_NEON_HAMMING_H

#include "cos_bsc.h"

#if defined(__aarch64__)
#include <arm_neon.h>

static inline uint32_t cos_hv_hamming_hw(const uint64_t *a, const uint64_t *b)
{
    uint32_t d = 0;
    for (int i = 0; i < COS_W; i += 2) {
        if (i + 4 < COS_W) {
            __builtin_prefetch((const void *)(a + i + 4), 0, 3);
            __builtin_prefetch((const void *)(b + i + 4), 0, 3);
        }
        uint64x2_t xa = vld1q_u64(a + i);
        uint64x2_t xb = vld1q_u64(b + i);
        uint64x2_t xo = veorq_u64(xa, xb);
        uint8x16_t bits = vreinterpretq_u8_u64(xo);
        uint8x16_t cnt = vcntq_u8(bits);
        d += (uint32_t)vaddlvq_u8(cnt);
    }
    return d;
}

#else

static inline uint32_t cos_hv_hamming_hw(const uint64_t *a, const uint64_t *b)
{
    return cos_hv_hamming(a, b);
}

#endif /* __aarch64__ */

#endif /* COS_NEON_HAMMING_H */
