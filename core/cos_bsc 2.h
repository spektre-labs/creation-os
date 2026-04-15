/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Shared BSC primitives for modular sources in this repository.
 * Canonical full demo: ../creation_os_v2.c
 */
#ifndef COS_BSC_H
#define COS_BSC_H

#include <math.h>
#include <stdint.h>
#include <string.h>

#define COS_D 4096
#define COS_W 64
#define COS_B (COS_W * 8)

static inline uint64_t cos_rng(uint64_t *state)
{
    *state = *state * 6364136223846793005ULL + 1442695040888963407ULL;
    return *state;
}

static inline void cos_hv_random(uint64_t *state, uint64_t *h)
{
    for (int i = 0; i < COS_W; i++)
        h[i] = cos_rng(state);
}

/** Scalar Hamming (reference). On AArch64, cos_neon_hamming.h adds cos_hv_hamming_hw / MAJ3 / bind / σ_hw. */
static inline uint32_t cos_hv_hamming(const uint64_t *a, const uint64_t *b)
{
    uint32_t d = 0;
    for (int i = 0; i < COS_W; i++)
        d += (uint32_t)__builtin_popcountll(a[i] ^ b[i]);
    return d;
}

static inline float cos_hv_sigma(const uint64_t *a, const uint64_t *b)
{
    float r = (float)cos_hv_hamming(a, b) / (float)COS_D;
    return r * r;
}

static inline void cos_hv_maj3(uint64_t *out, const uint64_t *a, const uint64_t *b, const uint64_t *c)
{
    for (int i = 0; i < COS_W; i++)
        out[i] = (a[i] & b[i]) | (a[i] & c[i]) | (b[i] & c[i]);
}

static inline void cos_hv_rotl(uint64_t *out, const uint64_t *in, int position)
{
    int shift = (position * 31) % COS_D;
    int word_shift = (shift / 64) % COS_W;
    int bit_shift = shift % 64;
    for (int i = 0; i < COS_W; i++) {
        int src = (i - word_shift + COS_W) % COS_W;
        int prev = (src - 1 + COS_W) % COS_W;
        out[i] = (bit_shift == 0) ? in[src]
                                  : (in[src] << bit_shift) | (in[prev] >> (64 - bit_shift));
    }
}

#endif /* COS_BSC_H */
