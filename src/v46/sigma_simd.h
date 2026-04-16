/* SPDX-License-Identifier: AGPL-3.0-or-later
 * v46 lab: small SIMD reductions for logits-row scans (optional; scalar fallback always available).
 */
#ifndef CREATION_OS_V46_SIGMA_SIMD_H
#define CREATION_OS_V46_SIGMA_SIMD_H

#include <stddef.h>
#include <stdint.h>

enum {
    V46_SIMD_NONE = 0,
    V46_SIMD_NEON = 1u << 0,
    V46_SIMD_AVX2 = 1u << 1,
};

uint32_t v46_simd_capabilities(void);

/** Sum of n floats; n must be >= 0. */
float v46_sum_f32_simd(const float *x, int n);
float v46_max_f32_simd(const float *x, int n);

#endif /* CREATION_OS_V46_SIGMA_SIMD_H */
