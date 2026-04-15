/*
 * CREATION OS — THE GENESIS BLOCK (Phase 0)
 * Immutable 128-bit DNA: Rainio | The Answer | Jobs mask | Synthesis opcode.
 * License: CC BY 4.0 | Lauri Elias Rainio | Helsinki | 1 = 1
 */
#import "creation_os_manifesto.h"
#import <arm_neon.h>
#import <cstring>
#import <stdint.h>

#if INVARIANT && defined(__aarch64__)

extern "C" {
alignas(16) const uint32_t GENESIS_DNA[4] = {
    0x11111111u,
    0x0000002Au,
    0xDEADBEEFu,
    0x19552011u,
};

void sk9_genesis_ignite(float *vector_128) {
    if (!vector_128) {
        return;
    }
    uint32x4_t dna = vld1q_u32(GENESIS_DNA);
    vst1q_f32(vector_128, vreinterpretq_f32_u32(dna));
}

int sk9_genesis_verify(const float *vector_128) {
    if (!vector_128) {
        return 0;
    }
    uint32x4_t current = vreinterpretq_u32_f32(vld1q_f32(vector_128));
    uint32x4_t dna      = vld1q_u32(GENESIS_DNA);
    uint32x4_t diff     = veorq_u32(current, dna);
    uint64_t   d0       = vgetq_lane_u64(vreinterpretq_u64_u32(diff), 0);
    uint64_t   d1       = vgetq_lane_u64(vreinterpretq_u64_u32(diff), 1);
    return (d0 | d1) == 0ull ? 1 : 0;
}
}

#else

extern "C" {
alignas(16) const uint32_t GENESIS_DNA[4] = {
    0x11111111u,
    0x0000002Au,
    0xDEADBEEFu,
    0x19552011u,
};

void sk9_genesis_ignite(float *vector_128) {
    if (!vector_128) {
        return;
    }
    memcpy(vector_128, GENESIS_DNA, 16);
}

int sk9_genesis_verify(const float *vector_128) {
    if (!vector_128) {
        return 0;
    }
    return memcmp(vector_128, GENESIS_DNA, 16) == 0 ? 1 : 0;
}
}

#endif
