/* SPDX-License-Identifier: AGPL-3.0-or-later
 * AArch64 NEON: 4096-bit hypervector primitives (Hamming, MAJ3 median form, XOR bind / in-place, σ, projector).
 * Scalar fallbacks match cos_bsc.h semantics for non-AArch64.
 */
#ifndef COS_NEON_HAMMING_H
#define COS_NEON_HAMMING_H

#include "cos_bsc.h"
#include <stdint.h>

#if defined(__aarch64__)
#include <arm_neon.h>

/** Hamming distance: vcnt per byte lane, vaddvq_u8 sums 16 popcounts (128 bits) per step. */
static inline uint32_t cos_hv_hamming_hw(const uint64_t *a, const uint64_t *b)
{
    uint32_t d = 0;
    int      i = 0;
    for (; i + 7 < COS_W; i += 8) {
        __builtin_prefetch((const void *)(a + i + 8), 0, 3);
        __builtin_prefetch((const void *)(b + i + 8), 0, 3);
        uint64x2_t x0 = veorq_u64(vld1q_u64(a + i), vld1q_u64(b + i));
        uint64x2_t x1 = veorq_u64(vld1q_u64(a + i + 2), vld1q_u64(b + i + 2));
        uint64x2_t x2 = veorq_u64(vld1q_u64(a + i + 4), vld1q_u64(b + i + 4));
        uint64x2_t x3 = veorq_u64(vld1q_u64(a + i + 6), vld1q_u64(b + i + 6));
        d += (uint32_t)vaddvq_u8(vcntq_u8(vreinterpretq_u8_u64(x0)));
        d += (uint32_t)vaddvq_u8(vcntq_u8(vreinterpretq_u8_u64(x1)));
        d += (uint32_t)vaddvq_u8(vcntq_u8(vreinterpretq_u8_u64(x2)));
        d += (uint32_t)vaddvq_u8(vcntq_u8(vreinterpretq_u8_u64(x3)));
    }
    return d;
}

static inline float cos_hv_sigma_hw(const uint64_t *a, const uint64_t *b)
{
    float r = (float)cos_hv_hamming_hw(a, b) / (float)COS_D;
    return r * r;
}

/** XOR bind: out = a XOR b, wide NEON stores. */
static inline void cos_hv_bind_xor_hw(uint64_t *out, const uint64_t *a, const uint64_t *b)
{
    int i = 0;
    for (; i + 7 < COS_W; i += 8) {
        __builtin_prefetch((const void *)(a + i + 8), 0, 3);
        __builtin_prefetch((const void *)(b + i + 8), 0, 3);
        uint64x2_t a0 = vld1q_u64(a + i), a1 = vld1q_u64(a + i + 2), a2 = vld1q_u64(a + i + 4), a3 = vld1q_u64(a + i + 6);
        uint64x2_t b0 = vld1q_u64(b + i), b1 = vld1q_u64(b + i + 2), b2 = vld1q_u64(b + i + 4), b3 = vld1q_u64(b + i + 6);
        vst1q_u64(out + i, veorq_u64(a0, b0));
        vst1q_u64(out + i + 2, veorq_u64(a1, b1));
        vst1q_u64(out + i + 4, veorq_u64(a2, b2));
        vst1q_u64(out + i + 6, veorq_u64(a3, b3));
    }
}

/** In-place XOR: io ^= x (bind stream into accumulator hypervector). */
static inline void cos_hv_xor_inplace_hw(uint64_t *io, const uint64_t *x)
{
    int i = 0;
    for (; i + 7 < COS_W; i += 8) {
        __builtin_prefetch((const void *)(x + i + 8), 0, 3);
        __builtin_prefetch((void *)(io + i + 8), 1, 3);
        uint64x2_t v0 = vld1q_u64(io + i), v1 = vld1q_u64(io + i + 2), v2 = vld1q_u64(io + i + 4), v3 = vld1q_u64(io + i + 6);
        uint64x2_t x0 = vld1q_u64(x + i), x1 = vld1q_u64(x + i + 2), x2 = vld1q_u64(x + i + 4), x3 = vld1q_u64(x + i + 6);
        vst1q_u64(io + i, veorq_u64(v0, x0));
        vst1q_u64(io + i + 2, veorq_u64(v1, x1));
        vst1q_u64(io + i + 4, veorq_u64(v2, x2));
        vst1q_u64(io + i + 6, veorq_u64(v3, x3));
    }
}

/**
 * MAJ3 bundle: per-bit majority — SIMD median form
 *   ((b & c) | a) & (b | c)  ≡  (a&b)|(a&c)|(b&c)
 * (standard bitwise-majority identity; one fewer AND than the naive expansion).
 */
static inline void cos_hv_maj3_hw(uint64_t *out, const uint64_t *a, const uint64_t *b, const uint64_t *c)
{
    int i = 0;
    for (; i + 7 < COS_W; i += 8) {
        __builtin_prefetch((const void *)(a + i + 8), 0, 3);
        __builtin_prefetch((const void *)(b + i + 8), 0, 3);
        __builtin_prefetch((const void *)(c + i + 8), 0, 3);
        uint64x2_t a0 = vld1q_u64(a + i), a1 = vld1q_u64(a + i + 2), a2 = vld1q_u64(a + i + 4), a3 = vld1q_u64(a + i + 6);
        uint64x2_t b0 = vld1q_u64(b + i), b1 = vld1q_u64(b + i + 2), b2 = vld1q_u64(b + i + 4), b3 = vld1q_u64(b + i + 6);
        uint64x2_t c0 = vld1q_u64(c + i), c1 = vld1q_u64(c + i + 2), c2 = vld1q_u64(c + i + 4), c3 = vld1q_u64(c + i + 6);
        uint64x2_t bc0 = vandq_u64(b0, c0), bc1 = vandq_u64(b1, c1), bc2 = vandq_u64(b2, c2), bc3 = vandq_u64(b3, c3);
        uint64x2_t o0 = vandq_u64(vorrq_u64(bc0, a0), vorrq_u64(b0, c0));
        uint64x2_t o1 = vandq_u64(vorrq_u64(bc1, a1), vorrq_u64(b1, c1));
        uint64x2_t o2 = vandq_u64(vorrq_u64(bc2, a2), vorrq_u64(b2, c2));
        uint64x2_t o3 = vandq_u64(vorrq_u64(bc3, a3), vorrq_u64(b3, c3));
        vst1q_u64(out + i, o0);
        vst1q_u64(out + i + 2, o1);
        vst1q_u64(out + i + 4, o2);
        vst1q_u64(out + i + 6, o3);
    }
}

/** Projector mismatch: popcount((h XOR anchor) AND trace_mask). */
static inline uint32_t cos_hv_measurement_mismatch_hw(const uint64_t *h, const uint64_t *anchor,
                                                      const uint64_t *trace_mask)
{
    uint32_t d = 0;
    int      i = 0;
    for (; i + 7 < COS_W; i += 8) {
        __builtin_prefetch((const void *)(h + i + 8), 0, 3);
        __builtin_prefetch((const void *)(anchor + i + 8), 0, 3);
        __builtin_prefetch((const void *)(trace_mask + i + 8), 0, 3);
        uint64x2_t d0 = veorq_u64(vld1q_u64(h + i), vld1q_u64(anchor + i));
        uint64x2_t d1 = veorq_u64(vld1q_u64(h + i + 2), vld1q_u64(anchor + i + 2));
        uint64x2_t d2 = veorq_u64(vld1q_u64(h + i + 4), vld1q_u64(anchor + i + 4));
        uint64x2_t d3 = veorq_u64(vld1q_u64(h + i + 6), vld1q_u64(anchor + i + 6));
        uint64x2_t t0 = vld1q_u64(trace_mask + i), t1 = vld1q_u64(trace_mask + i + 2);
        uint64x2_t t2 = vld1q_u64(trace_mask + i + 4), t3 = vld1q_u64(trace_mask + i + 6);
        uint64x2_t m0 = vandq_u64(d0, t0), m1 = vandq_u64(d1, t1), m2 = vandq_u64(d2, t2), m3 = vandq_u64(d3, t3);
        d += (uint32_t)vaddvq_u8(vcntq_u8(vreinterpretq_u8_u64(m0)));
        d += (uint32_t)vaddvq_u8(vcntq_u8(vreinterpretq_u8_u64(m1)));
        d += (uint32_t)vaddvq_u8(vcntq_u8(vreinterpretq_u8_u64(m2)));
        d += (uint32_t)vaddvq_u8(vcntq_u8(vreinterpretq_u8_u64(m3)));
    }
    return d;
}

#else

static inline uint32_t cos_hv_hamming_hw(const uint64_t *a, const uint64_t *b)
{
    return cos_hv_hamming(a, b);
}

static inline float cos_hv_sigma_hw(const uint64_t *a, const uint64_t *b)
{
    return cos_hv_sigma(a, b);
}

static inline void cos_hv_bind_xor_hw(uint64_t *out, const uint64_t *a, const uint64_t *b)
{
    for (int i = 0; i < COS_W; i++)
        out[i] = a[i] ^ b[i];
}

static inline void cos_hv_xor_inplace_hw(uint64_t *io, const uint64_t *x)
{
    for (int i = 0; i < COS_W; i++)
        io[i] ^= x[i];
}

static inline void cos_hv_maj3_hw(uint64_t *out, const uint64_t *a, const uint64_t *b, const uint64_t *c)
{
    cos_hv_maj3(out, a, b, c);
}

static inline uint32_t cos_hv_measurement_mismatch_hw(const uint64_t *h, const uint64_t *anchor,
                                                      const uint64_t *trace_mask)
{
    uint32_t d = 0;
    for (int i = 0; i < COS_W; i++) {
        uint64_t bits = (h[i] ^ anchor[i]) & trace_mask[i];
        d += (uint32_t)__builtin_popcountll(bits);
    }
    return d;
}

#endif /* __aarch64__ */

#endif /* COS_NEON_HAMMING_H */
