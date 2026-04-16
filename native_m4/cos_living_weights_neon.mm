/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "creation_os_native_m4.h"

#include <stdint.h>

#if defined(__ARM_NEON) || defined(__ARM_NEON__)
#include <arm_neon.h>
#endif

void cos_living_weights_neon(float *logits, const uint8_t *reputation, int vocab, float scale)
{
    if (!logits || !reputation || vocab <= 0)
        return;

#if defined(__ARM_NEON) || defined(__ARM_NEON__)
    const float32x4_t c4 = vdupq_n_f32(4.0f);
    const float32x4_t sc = vdupq_n_f32(scale);
    int i = 0;
    for (; i + 16 <= vocab; i += 16) {
        uint8x16_t r16 = vld1q_u8(reputation + i);

        for (int half = 0; half < 2; half++) {
            uint8x8_t r8 = (half == 0) ? vget_low_u8(r16) : vget_high_u8(r16);
            uint8x8_t c8 = vcnt_u8(r8);
            uint16x8_t c16 = vmovl_u8(c8);

            uint32x4_t c32_lo = vmovl_u16(vget_low_u16(c16));
            uint32x4_t c32_hi = vmovl_u16(vget_high_u16(c16));

            float32x4_t pc_lo = vcvtq_f32_u32(c32_lo);
            float32x4_t pc_hi = vcvtq_f32_u32(c32_hi);

            float32x4_t add_lo = vmulq_f32(vsubq_f32(pc_lo, c4), sc);
            float32x4_t add_hi = vmulq_f32(vsubq_f32(pc_hi, c4), sc);

            int base = i + half * 8;
            float32x4_t l0 = vld1q_f32(logits + base + 0);
            float32x4_t l1 = vld1q_f32(logits + base + 4);
            vst1q_f32(logits + base + 0, vaddq_f32(l0, add_lo));
            vst1q_f32(logits + base + 4, vaddq_f32(l1, add_hi));
        }
    }
    for (; i < vocab; i++) {
        int pc = __builtin_popcount((unsigned)reputation[i]);
        logits[i] += ((float)pc - 4.0f) * scale;
    }
#else
    for (int i = 0; i < vocab; i++) {
        int pc = __builtin_popcount((unsigned)reputation[i]);
        logits[i] += ((float)pc - 4.0f) * scale;
    }
#endif
}
