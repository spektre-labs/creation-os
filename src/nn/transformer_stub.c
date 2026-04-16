/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Toy dense forward used by v28 self-tests (not BitNet-2B).
 * On AArch64, inner GEMV uses NEON with four accumulators + prefetch (Apple Silicon–friendly).
 */
#include "transformer_stub.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

#if defined(__aarch64__) && defined(__ARM_NEON)
#include <arm_neon.h>
#endif

void cos_nn_toy_linear_f32(const float *w /* nout x nin row-major */, const float *x, int nin, int nout, float *y)
{
#if defined(__aarch64__) && defined(__ARM_NEON)
    for (int r = 0; r < nout; r++) {
        const float *wr = w + (ptrdiff_t)r * nin;
        float32x4_t s0 = vdupq_n_f32(0.f);
        float32x4_t s1 = vdupq_n_f32(0.f);
        float32x4_t s2 = vdupq_n_f32(0.f);
        float32x4_t s3 = vdupq_n_f32(0.f);
        int c = 0;
        for (; c + 16 <= nin; c += 16) {
            __builtin_prefetch(wr + c + 64, 0, 3);
            __builtin_prefetch(x + c + 64, 0, 3);
            float32x4_t xc0 = vld1q_f32(x + c);
            float32x4_t xc4 = vld1q_f32(x + c + 4);
            float32x4_t xc8 = vld1q_f32(x + c + 8);
            float32x4_t x12 = vld1q_f32(x + c + 12);
            s0 = vfmaq_f32(s0, vld1q_f32(wr + c), xc0);
            s1 = vfmaq_f32(s1, vld1q_f32(wr + c + 4), xc4);
            s2 = vfmaq_f32(s2, vld1q_f32(wr + c + 8), xc8);
            s3 = vfmaq_f32(s3, vld1q_f32(wr + c + 12), x12);
        }
        float32x4_t t01 = vaddq_f32(s0, s1);
        float32x4_t t23 = vaddq_f32(s2, s3);
        float acc = vaddvq_f32(vaddq_f32(t01, t23));
        for (; c < nin; c++)
            acc += wr[c] * x[c];
        y[r] = acc;
    }
#else
    for (int r = 0; r < nout; r++) {
        float acc = 0.f;
        for (int c = 0; c < nin; c++)
            acc += w[r * nin + c] * x[c];
        y[r] = acc;
    }
#endif
}

float cos_nn_squared_relu(float x)
{
    float t = x > 0.f ? x : 0.f;
    return t * t;
}
