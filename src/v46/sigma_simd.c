/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "sigma_simd.h"

#include <math.h>

#if defined(__aarch64__) && defined(__ARM_NEON)
#include <arm_neon.h>
static float v46_sum_neon(const float *x, int n)
{
    float32x4_t acc = vdupq_n_f32(0.0f);
    int i = 0;
    for (; i + 4 <= n; i += 4) {
        float32x4_t v = vld1q_f32(x + i);
        acc = vaddq_f32(acc, v);
    }
    float s = vaddvq_f32(acc);
    for (; i < n; i++) {
        s += x[i];
    }
    return s;
}

static float v46_max_neon(const float *x, int n)
{
    if (!x || n <= 0) {
        return 0.0f;
    }
    float32x4_t m4 = vld1q_dup_f32(x);
    int i = 0;
    for (; i + 4 <= n; i += 4) {
        float32x4_t v = vld1q_f32(x + i);
        m4 = vmaxq_f32(m4, v);
    }
    float m = vmaxvq_f32(m4);
    for (; i < n; i++) {
        if (x[i] > m) {
            m = x[i];
        }
    }
    return m;
}
#endif

#if defined(__AVX2__)
#include <immintrin.h>

static float v46_hsum256_ps(__m256 v)
{
    __m128 lo = _mm256_castps256_ps128(v);
    __m128 hi = _mm256_extractf128_ps(v, 1);
    __m128 s = _mm_add_ps(lo, hi);
    s = _mm_hadd_ps(s, s);
    s = _mm_hadd_ps(s, s);
    return _mm_cvtss_f32(s);
}

static float v46_sum_avx2(const float *x, int n)
{
    __m256 acc = _mm256_setzero_ps();
    int i = 0;
    for (; i + 8 <= n; i += 8) {
        __m256 v = _mm256_loadu_ps(x + i);
        acc = _mm256_add_ps(acc, v);
    }
    float s = v46_hsum256_ps(acc);
    for (; i < n; i++) {
        s += x[i];
    }
    return s;
}

static float v46_max_avx2(const float *x, int n)
{
    if (!x || n <= 0) {
        return 0.0f;
    }
    __m256 m8 = _mm256_set1_ps(x[0]);
    int i = 0;
    for (; i + 8 <= n; i += 8) {
        __m256 v = _mm256_loadu_ps(x + i);
        m8 = _mm256_max_ps(m8, v);
    }
    __m128 lo = _mm256_castps256_ps128(m8);
    __m128 hi = _mm256_extractf128_ps(m8, 1);
    __m128 m4 = _mm_max_ps(lo, hi);
    m4 = _mm_max_ps(m4, _mm_movehl_ps(m4, m4));
    m4 = _mm_max_ss(m4, _mm_movehdup_ps(m4));
    float m = _mm_cvtss_f32(m4);
    for (; i < n; i++) {
        if (x[i] > m) {
            m = x[i];
        }
    }
    return m;
}
#endif

uint32_t v46_simd_capabilities(void)
{
    uint32_t f = V46_SIMD_NONE;
#if defined(__aarch64__) && defined(__ARM_NEON)
    f |= V46_SIMD_NEON;
#endif
#if defined(__AVX2__)
    f |= V46_SIMD_AVX2;
#endif
    return f;
}

float v46_sum_f32_simd(const float *x, int n)
{
    if (!x || n <= 0) {
        return 0.0f;
    }
#if defined(__aarch64__) && defined(__ARM_NEON)
    return v46_sum_neon(x, n);
#elif defined(__AVX2__)
    return v46_sum_avx2(x, n);
#else
    {
        float s = 0.0f;
        for (int i = 0; i < n; i++) {
            s += x[i];
        }
        return s;
    }
#endif
}

float v46_max_f32_simd(const float *x, int n)
{
    if (!x || n <= 0) {
        return 0.0f;
    }
#if defined(__aarch64__) && defined(__ARM_NEON)
    return v46_max_neon(x, n);
#elif defined(__AVX2__)
    return v46_max_avx2(x, n);
#else
    {
        float m = x[0];
        for (int i = 1; i < n; i++) {
            if (x[i] > m) {
                m = x[i];
            }
        }
        return m;
    }
#endif
}
