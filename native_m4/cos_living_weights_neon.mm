/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "creation_os_native_m4.h"

#include <stdint.h>

#if defined(__APPLE__)
#include <dispatch/dispatch.h>
#endif

#if defined(__ARM_NEON) || defined(__ARM_NEON__)
#include <arm_neon.h>

/* One contiguous 16-token block: logits[0..15] += f(popcount(rep[0..15])). */
static inline void cos_lw_neon_accum_16(float *logits, const uint8_t *rep16, float32x4_t c4, float32x4_t sc)
{
    uint8x16_t r = vld1q_u8(rep16);
    uint8x16_t c = vcntq_u8(r);

    uint16x8_t c16_lo = vmovl_u8(vget_low_u8(c));
    uint16x8_t c16_hi = vmovl_u8(vget_high_u8(c));

    uint32x4_t c0 = vmovl_u16(vget_low_u16(c16_lo));
    uint32x4_t c1 = vmovl_u16(vget_high_u16(c16_lo));
    uint32x4_t c2 = vmovl_u16(vget_low_u16(c16_hi));
    uint32x4_t c3 = vmovl_u16(vget_high_u16(c16_hi));

    float32x4_t pc0 = vcvtq_f32_u32(c0);
    float32x4_t pc1 = vcvtq_f32_u32(c1);
    float32x4_t pc2 = vcvtq_f32_u32(c2);
    float32x4_t pc3 = vcvtq_f32_u32(c3);

    float32x4_t add0 = vmulq_f32(vsubq_f32(pc0, c4), sc);
    float32x4_t add1 = vmulq_f32(vsubq_f32(pc1, c4), sc);
    float32x4_t add2 = vmulq_f32(vsubq_f32(pc2, c4), sc);
    float32x4_t add3 = vmulq_f32(vsubq_f32(pc3, c4), sc);

    float32x4_t l0 = vld1q_f32(logits + 0);
    float32x4_t l1 = vld1q_f32(logits + 4);
    float32x4_t l2 = vld1q_f32(logits + 8);
    float32x4_t l3 = vld1q_f32(logits + 12);
    vst1q_f32(logits + 0, vaddq_f32(l0, add0));
    vst1q_f32(logits + 4, vaddq_f32(l1, add1));
    vst1q_f32(logits + 8, vaddq_f32(l2, add2));
    vst1q_f32(logits + 12, vaddq_f32(l3, add3));
}
#endif

void cos_living_weights_neon_range(float *logits, const uint8_t *reputation, int begin, int end, float scale)
{
    if (!logits || !reputation)
        return;
    if (begin < 0 || end <= begin)
        return;

#if defined(__ARM_NEON) || defined(__ARM_NEON__)
    const float32x4_t c4 = vdupq_n_f32(4.0f);
    const float32x4_t sc = vdupq_n_f32(scale);

    int i = begin;
    /* 64 bytes / iter: four 16-token blocks (prefetch amortized). */
    for (; i + 64 <= end; i += 64) {
        __builtin_prefetch(reputation + i + 256, 0, 3);
        __builtin_prefetch(logits + i + 256, 1, 3);

        cos_lw_neon_accum_16(logits + i + 0, reputation + i + 0, c4, sc);
        cos_lw_neon_accum_16(logits + i + 16, reputation + i + 16, c4, sc);
        cos_lw_neon_accum_16(logits + i + 32, reputation + i + 32, c4, sc);
        cos_lw_neon_accum_16(logits + i + 48, reputation + i + 48, c4, sc);
    }

    /* 16..63 byte tail: SIMD wide, then scalar remainder. */
    for (; i + 16 <= end; i += 16)
        cos_lw_neon_accum_16(logits + i, reputation + i, c4, sc);

    for (; i < end; i++) {
        int pc = __builtin_popcount((unsigned)reputation[i]);
        logits[i] += ((float)pc - 4.0f) * scale;
    }
#else
    for (int i = begin; i < end; i++) {
        int pc = __builtin_popcount((unsigned)reputation[i]);
        logits[i] += ((float)pc - 4.0f) * scale;
    }
#endif
}

void cos_living_weights_neon_parallel(float *logits, const uint8_t *reputation, int vocab, float scale)
{
#if defined(__APPLE__) && (defined(__ARM_NEON) || defined(__ARM_NEON__))
    if (!logits || !reputation || vocab <= 0)
        return;
    if (vocab < 65536) {
        cos_living_weights_neon_range(logits, reputation, 0, vocab, scale);
        return;
    }

    const int chunk = 65536; /* 64Ki tokens / chunk */
    const int n64 = (vocab / 64) * 64;
    const int chunks = (n64 + chunk - 1) / chunk;

    dispatch_apply((size_t)chunks, dispatch_get_global_queue(QOS_CLASS_USER_INTERACTIVE, 0), ^(size_t c) {
        int b = (int)c * chunk;
        int e = b + chunk;
        if (e > n64)
            e = n64;
        cos_living_weights_neon_range(logits, reputation, b, e, scale);
    });

    cos_living_weights_neon_range(logits, reputation, n64, vocab, scale);
#else
    cos_living_weights_neon_range(logits, reputation, 0, vocab, scale);
#endif
}

void cos_living_weights_neon(float *logits, const uint8_t *reputation, int vocab, float scale)
{
    if (!logits || !reputation || vocab <= 0)
        return;

#if defined(__APPLE__) && (defined(__ARM_NEON) || defined(__ARM_NEON__))
    if (vocab >= 65536) {
        cos_living_weights_neon_parallel(logits, reputation, vocab, scale);
        return;
    }
#endif

    cos_living_weights_neon_range(logits, reputation, 0, vocab, scale);
}
