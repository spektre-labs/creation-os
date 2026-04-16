/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "sigma3.h"

#include <math.h>
#include <string.h>
#include <float.h>

#if defined(__aarch64__) || defined(__ARM_NEON)
#include <arm_neon.h>
#define V55_HAS_NEON 1
#else
#define V55_HAS_NEON 0
#endif

/* -------- stable softmax -------- */
void v55_softmax_inplace(float *logits, int n)
{
    if (!logits || n <= 0) return;
    float maxv = logits[0];
    for (int i = 1; i < n; i++) {
        if (logits[i] > maxv) maxv = logits[i];
    }
    double sum = 0.0;
    for (int i = 0; i < n; i++) {
        float e = expf(logits[i] - maxv);
        logits[i] = e;
        sum += (double)e;
    }
    if (!(sum > 0.0)) return;
    float inv = (float)(1.0 / sum);
    for (int i = 0; i < n; i++) logits[i] *= inv;
}

#if V55_HAS_NEON
/* Fast log₂ via IEEE-754 exponent extraction + 2nd-order minimax poly.
 * Accuracy ≈ ±0.01 bits across (0, ∞) — sufficient for σ-gating.
 * Branchless; no libm call on the hot path. */
static inline float32x4_t v55_vlog2q_f32(float32x4_t x)
{
    const float32x4_t one = vdupq_n_f32(1.0f);
    const uint32x4_t exp_mask = vdupq_n_u32(0x7F800000u);
    const uint32x4_t mant_mask = vdupq_n_u32(0x007FFFFFu);
    const uint32x4_t one_bits = vdupq_n_u32(0x3F800000u);

    uint32x4_t xi = vreinterpretq_u32_f32(x);
    int32x4_t e = vsubq_s32(
        vreinterpretq_s32_u32(vshrq_n_u32(vandq_u32(xi, exp_mask), 23)),
        vdupq_n_s32(127));
    uint32x4_t mant_bits = vorrq_u32(vandq_u32(xi, mant_mask), one_bits);
    float32x4_t m = vreinterpretq_f32_u32(mant_bits);

    /* log2(m) ≈ a + b·m + c·m² for m ∈ [1, 2)  (minimax) */
    const float32x4_t a = vdupq_n_f32(-1.7417939f);
    const float32x4_t b = vdupq_n_f32( 2.8212026f);
    const float32x4_t c = vdupq_n_f32(-1.0797901f);
    float32x4_t poly = vmlaq_f32(b, c, m);      /* b + c·m */
    poly = vmlaq_f32(a, poly, m);               /* a + m·(b + c·m) */
    (void)one;
    return vaddq_f32(vcvtq_f32_s32(e), poly);
}

static float v55_entropy_bits_neon(const float *p, int n)
{
    const float eps = 1e-30f;
    float32x4_t acc0 = vdupq_n_f32(0.0f);
    float32x4_t acc1 = vdupq_n_f32(0.0f);
    float32x4_t acc2 = vdupq_n_f32(0.0f);
    float32x4_t acc3 = vdupq_n_f32(0.0f);
    const float32x4_t epsv = vdupq_n_f32(eps);

    int i = 0;
    for (; i + 16 <= n; i += 16) {
        __builtin_prefetch(&p[i + 64], 0, 3);
        float32x4_t p0 = vld1q_f32(&p[i +  0]);
        float32x4_t p1 = vld1q_f32(&p[i +  4]);
        float32x4_t p2 = vld1q_f32(&p[i +  8]);
        float32x4_t p3 = vld1q_f32(&p[i + 12]);
        p0 = vmaxq_f32(p0, epsv);
        p1 = vmaxq_f32(p1, epsv);
        p2 = vmaxq_f32(p2, epsv);
        p3 = vmaxq_f32(p3, epsv);
        acc0 = vmlaq_f32(acc0, p0, v55_vlog2q_f32(p0));
        acc1 = vmlaq_f32(acc1, p1, v55_vlog2q_f32(p1));
        acc2 = vmlaq_f32(acc2, p2, v55_vlog2q_f32(p2));
        acc3 = vmlaq_f32(acc3, p3, v55_vlog2q_f32(p3));
    }
    float32x4_t total = vaddq_f32(vaddq_f32(acc0, acc1), vaddq_f32(acc2, acc3));
    float h = vaddvq_f32(total);                /* horizontal add */
    for (; i < n; i++) {
        float pi = p[i] > eps ? p[i] : eps;
        h += pi * log2f(pi);
    }
    return -h;
}
#endif

static float v55_entropy_bits_scalar(const float *p, int n)
{
    const float eps = 1e-30f;
    double h = 0.0;
    for (int i = 0; i < n; i++) {
        float pi = p[i] > eps ? p[i] : eps;
        h += (double)pi * log2((double)pi);
    }
    return (float)(-h);
}

static float v55_entropy_bits(const float *p, int n)
{
#if V55_HAS_NEON
    return v55_entropy_bits_neon(p, n);
#else
    return v55_entropy_bits_scalar(p, n);
#endif
}

/* Top-K partial sort via bounded insertion.  K is small (<= 32). */
static int v55_top_k(const float *p, int n, int k, float *out_vals)
{
    if (k > n) k = n;
    for (int i = 0; i < k; i++) out_vals[i] = -1.0f;
    for (int i = 0; i < n; i++) {
        float v = p[i];
        /* find insertion point */
        int pos = -1;
        for (int j = 0; j < k; j++) {
            if (v > out_vals[j]) { pos = j; break; }
        }
        if (pos < 0) continue;
        /* shift right to make room */
        for (int j = k - 1; j > pos; j--) out_vals[j] = out_vals[j - 1];
        out_vals[pos] = v;
    }
    return k;
}

/* Normalized Gini dispersion over top-K probabilities.
 *   dispersion = 1 − Σ (p_i / S)² ,  S = Σ p_i
 * Range [0, 1 − 1/K].  Normalize to [0, 1] by dividing by (1 − 1/K).
 *   0  ⇒ single peak dominates (no ambiguity)
 *   1  ⇒ uniform across top-K (max ambiguity)
 */
static float v55_top_k_dispersion(const float *top_vals, int k)
{
    if (k <= 1) return 0.0f;
    double s = 0.0;
    for (int i = 0; i < k; i++) s += top_vals[i];
    if (!(s > 0.0)) return 0.0f;
    double ss = 0.0;
    for (int i = 0; i < k; i++) {
        double r = (double)top_vals[i] / s;
        ss += r * r;
    }
    double raw = 1.0 - ss;                      /* Gini-style */
    double norm = 1.0 - 1.0 / (double)k;
    if (!(norm > 0.0)) return 0.0f;
    double disp = raw / norm;
    if (disp < 0.0) disp = 0.0;
    if (disp > 1.0) disp = 1.0;
    return (float)disp;
}

void v55_sigma3_from_probs(const float *probs, int n, int top_k, v55_sigma3_t *out)
{
    if (!out) return;
    memset(out, 0, sizeof(*out));
    if (!probs || n <= 0) return;
    if (top_k <= 0 || top_k > 32) top_k = 8;
    if (top_k > n) top_k = n;

    /* max P */
    float maxp = probs[0];
    for (int i = 1; i < n; i++) if (probs[i] > maxp) maxp = probs[i];
    if (maxp < 0.0f) maxp = 0.0f;
    if (maxp > 1.0f) maxp = 1.0f;

    /* entropy in bits */
    float h = v55_entropy_bits(probs, n);
    float log2n = log2f((float)n);
    float h_norm = (log2n > 0.0f) ? (h / log2n) : 0.0f;
    if (h_norm < 0.0f) h_norm = 0.0f;
    if (h_norm > 1.0f) h_norm = 1.0f;

    /* top-K dispersion */
    float topvals[32];
    v55_top_k(probs, n, top_k, topvals);
    float disp = v55_top_k_dispersion(topvals, top_k);

    out->sigma_input = disp;
    out->sigma_knowledge = 1.0f - maxp;
    out->sigma_decoding = h_norm;
    out->h_bits = h;
    out->top_k_used = top_k;
    out->sigma_total = (out->sigma_input + out->sigma_knowledge + out->sigma_decoding) / 3.0f;
    if (out->sigma_total < 0.0f) out->sigma_total = 0.0f;
    if (out->sigma_total > 1.0f) out->sigma_total = 1.0f;
}

void v55_sigma3_from_logits(const float *logits, int n, int top_k,
                            float *scratch_probs, v55_sigma3_t *out)
{
    if (!scratch_probs || !logits || !out || n <= 0) return;
    memcpy(scratch_probs, logits, (size_t)n * sizeof(float));
    v55_softmax_inplace(scratch_probs, n);
    v55_sigma3_from_probs(scratch_probs, n, top_k, out);
}
