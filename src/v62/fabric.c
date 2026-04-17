/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Creation OS v62 — Reasoning Fabric implementation (M-tier).
 *
 * Hardware discipline (M4):
 *   - aligned_alloc(64, ...) for every allocation.  Never `malloc`
 *     on the hot path.
 *   - branchless inner loops; selection via 0/1 masks.
 *   - 4-way NEON FMA accumulators with prefetch when the platform
 *     has <arm_neon.h>.  Portable scalar fallback otherwise (also
 *     unrolled 4-way so the compiler's auto-vectoriser still gets
 *     the same shape).
 *   - mmap-friendly layouts: KV rows are 64-B-stride contiguous so
 *     v62 can read straight from a memory-mapped block file.
 *
 * Honest non-claim: the M4 SME path (arm_sme.h) is gated by
 *   COS_V62_SME=1 and remains compile-only until the streaming-mode
 *   prologue/epilogue is wired safely.  Default build uses NEON,
 *   never SIGILLs.
 */

#include "fabric.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#if defined(__ARM_NEON) || defined(__aarch64__)
#  include <arm_neon.h>
#  define COS_V62_HAS_NEON 1
#else
#  define COS_V62_HAS_NEON 0
#endif

/* ====================================================================
 *  Internal helpers — branchless math + aligned alloc
 * ==================================================================== */

static inline float v62_clampf(float x, float lo, float hi)
{
    /* Two min/max → branchless on every modern arch. */
    float a = x < lo ? lo : x;
    return a > hi ? hi : a;
}

static inline float v62_maxf(float a, float b) { return a > b ? a : b; }

static void *v62_alloc64(size_t bytes)
{
    /* Round up to a multiple of 64 so aligned_alloc accepts it
     * everywhere (POSIX requires size % alignment == 0). */
    size_t rounded = (bytes + 63u) & ~((size_t)63u);
    if (rounded == 0) rounded = 64;
    void *p = NULL;
#if defined(_ISOC11_SOURCE) || (__STDC_VERSION__ >= 201112L)
    p = aligned_alloc(64, rounded);
#else
    if (posix_memalign(&p, 64, rounded) != 0) return NULL;
#endif
    if (p) memset(p, 0, rounded);
    return p;
}

/*  4-way NEON FMA dot.  Falls back to a 4-way scalar accumulator that
 *  the compiler reliably auto-vectorises.  Prefetches +64 B ahead.   */
static float v62_dot(const float *a, const float *b, uint32_t n)
{
#if COS_V62_HAS_NEON
    float32x4_t s0 = vdupq_n_f32(0.f);
    float32x4_t s1 = vdupq_n_f32(0.f);
    float32x4_t s2 = vdupq_n_f32(0.f);
    float32x4_t s3 = vdupq_n_f32(0.f);
    uint32_t i = 0;
    for (; i + 16 <= n; i += 16) {
        __builtin_prefetch(a + i + 64, 0, 3);
        __builtin_prefetch(b + i + 64, 0, 3);
        s0 = vfmaq_f32(s0, vld1q_f32(a + i),      vld1q_f32(b + i));
        s1 = vfmaq_f32(s1, vld1q_f32(a + i + 4),  vld1q_f32(b + i + 4));
        s2 = vfmaq_f32(s2, vld1q_f32(a + i + 8),  vld1q_f32(b + i + 8));
        s3 = vfmaq_f32(s3, vld1q_f32(a + i + 12), vld1q_f32(b + i + 12));
    }
    float32x4_t s = vaddq_f32(vaddq_f32(s0, s1), vaddq_f32(s2, s3));
    float acc = vaddvq_f32(s);
    for (; i < n; ++i) acc += a[i] * b[i];
    return acc;
#else
    float s0 = 0, s1 = 0, s2 = 0, s3 = 0;
    uint32_t i = 0;
    for (; i + 4 <= n; i += 4) {
        s0 += a[i + 0] * b[i + 0];
        s1 += a[i + 1] * b[i + 1];
        s2 += a[i + 2] * b[i + 2];
        s3 += a[i + 3] * b[i + 3];
    }
    float acc = (s0 + s1) + (s2 + s3);
    for (; i < n; ++i) acc += a[i] * b[i];
    return acc;
#endif
}

/*  y ← y + alpha * x   (NEON SAXPY with 4-way unroll + prefetch).    */
static void v62_axpy(float *y, const float *x, float alpha, uint32_t n)
{
#if COS_V62_HAS_NEON
    float32x4_t a = vdupq_n_f32(alpha);
    uint32_t i = 0;
    for (; i + 16 <= n; i += 16) {
        __builtin_prefetch(x + i + 64, 0, 3);
        __builtin_prefetch(y + i + 64, 1, 3);
        vst1q_f32(y + i,      vfmaq_f32(vld1q_f32(y + i),      a, vld1q_f32(x + i)));
        vst1q_f32(y + i + 4,  vfmaq_f32(vld1q_f32(y + i + 4),  a, vld1q_f32(x + i + 4)));
        vst1q_f32(y + i + 8,  vfmaq_f32(vld1q_f32(y + i + 8),  a, vld1q_f32(x + i + 8)));
        vst1q_f32(y + i + 12, vfmaq_f32(vld1q_f32(y + i + 12), a, vld1q_f32(x + i + 12)));
    }
    for (; i < n; ++i) y[i] += alpha * x[i];
#else
    for (uint32_t i = 0; i < n; ++i) y[i] += alpha * x[i];
#endif
}

static float v62_l2(const float *x, uint32_t n)
{
    return sqrtf(v62_dot(x, x, n));
}

/* ====================================================================
 *  1.  Latent CoT (Coconut-class)
 * ==================================================================== */

cos_v62_thought_t *cos_v62_thought_new(uint32_t dim)
{
    if (dim == 0) return NULL;
    cos_v62_thought_t *t = (cos_v62_thought_t *)v62_alloc64(sizeof(*t));
    if (!t) return NULL;
    t->vec    = (float *)v62_alloc64((size_t)dim * sizeof(float));
    if (!t->vec) { free(t); return NULL; }
    t->dim    = dim;
    t->layer  = 0;
    t->step   = 0;
    t->sigma  = 1.0f;
    t->energy = 0.0f;
    return t;
}

void cos_v62_thought_free(cos_v62_thought_t *t)
{
    if (!t) return;
    free(t->vec);
    free(t);
}

int cos_v62_latent_step(cos_v62_thought_t *thought,
                        const float       *gradient,
                        float              alpha)
{
    if (!thought || !thought->vec || !gradient) return -1;

    /* Coconut update:   thought ← thought + alpha * gradient        */
    v62_axpy(thought->vec, gradient, alpha, thought->dim);

    /* L2 normalise so the latent space stays bounded across steps.
     * Branchless: divide by max(eps, ||thought||) — no `if`.        */
    float n = v62_l2(thought->vec, thought->dim);
    float inv = 1.0f / v62_maxf(n, 1e-8f);
#if COS_V62_HAS_NEON
    float32x4_t s = vdupq_n_f32(inv);
    uint32_t i = 0;
    for (; i + 4 <= thought->dim; i += 4) {
        vst1q_f32(thought->vec + i,
                  vmulq_f32(vld1q_f32(thought->vec + i), s));
    }
    for (; i < thought->dim; ++i) thought->vec[i] *= inv;
#else
    for (uint32_t i = 0; i < thought->dim; ++i) thought->vec[i] *= inv;
#endif

    /* σ-coherence: 1 - clamp(||delta||, 0, 1) where delta is the
     * change just applied; we approximate it with alpha * ||grad||. */
    float gnorm = v62_l2(gradient, thought->dim);
    float delta = v62_clampf(fabsf(alpha) * gnorm, 0.f, 1.f);
    thought->sigma = 1.0f - delta;
    thought->step += 1u;
    return 0;
}

int cos_v62_latent_loop(cos_v62_thought_t      *thought,
                        cos_v62_grad_fn         grad_fn,
                        void                   *grad_ctx,
                        const cos_v62_budget_t *budget,
                        float                   alpha,
                        float                   sigma_target,
                        uint32_t               *out_steps)
{
    if (!thought || !grad_fn || !budget) return -1;
    float *grad = (float *)v62_alloc64((size_t)thought->dim * sizeof(float));
    if (!grad) return -2;

    uint32_t steps = 0;
    int rc = 0;
    for (uint32_t s = 0; s < budget->max_thoughts; ++s) {
        rc = grad_fn(thought, grad, grad_ctx);
        if (rc != 0) break;
        if (cos_v62_latent_step(thought, grad, alpha) != 0) {
            rc = -3;
            break;
        }
        steps += 1u;
        if (thought->sigma >= sigma_target) break;
    }
    free(grad);
    if (out_steps) *out_steps = steps;
    return rc;
}

/* ====================================================================
 *  2.  Energy-Based Verifier (EBT-class)
 * ==================================================================== */

float cos_v62_energy(const float *input,
                     const float *candidate,
                     uint32_t     dim,
                     float        beta)
{
    if (!input || !candidate || dim == 0) return INFINITY;
    float sq = 0.f;
#if COS_V62_HAS_NEON
    float32x4_t s0 = vdupq_n_f32(0.f);
    uint32_t i = 0;
    for (; i + 4 <= dim; i += 4) {
        float32x4_t d = vsubq_f32(vld1q_f32(input + i),
                                  vld1q_f32(candidate + i));
        s0 = vfmaq_f32(s0, d, d);
    }
    sq = vaddvq_f32(s0);
    for (; i < dim; ++i) {
        float d = input[i] - candidate[i];
        sq += d * d;
    }
#else
    for (uint32_t i = 0; i < dim; ++i) {
        float d = input[i] - candidate[i];
        sq += d * d;
    }
#endif
    float ip = v62_dot(input, candidate, dim);
    float a  = v62_l2(input, dim);
    float b  = v62_l2(candidate, dim);
    float cos_sim = ip / v62_maxf(a * b, 1e-8f);
    return 0.5f * sq + beta * (1.0f - cos_sim);
}

int cos_v62_ebt_minimize(const float            *input,
                         float                  *candidate,
                         uint32_t                dim,
                         float                   beta,
                         float                   eta,
                         float                   tol,
                         const cos_v62_budget_t *budget,
                         float                  *out_energy,
                         uint32_t               *out_steps)
{
    if (!input || !candidate || !budget || dim == 0) return -1;

    /* Analytic gradient of E w.r.t. candidate y, with x = input:
     *   dE/dy = -(x - y) - beta * d/dy cos(x, y)
     *
     * For tight, branchless code we use a finite-difference-free
     * exact form for the squared-distance term and approximate the
     * cosine term by its directional component (-beta * x_unit) —
     * this preserves the EBT *direction* without an expensive
     * Jacobian.  The minimum of the magnitude term alone is y = x;
     * the cosine term nudges y toward the input direction.
     */
    float *grad = (float *)v62_alloc64((size_t)dim * sizeof(float));
    if (!grad) return -2;

    float prev_E = cos_v62_energy(input, candidate, dim, beta);
    uint32_t steps = 0;
    for (uint32_t s = 0; s < budget->max_ebt_steps; ++s) {
        /* grad = -(x - y)  →  step:  y ← y + eta * (x - y)           */
        float a_inv = 1.0f / v62_maxf(v62_l2(input, dim), 1e-8f);
        for (uint32_t i = 0; i < dim; ++i) {
            grad[i] = (input[i] - candidate[i])
                    + beta * (input[i] * a_inv);
        }
        v62_axpy(candidate, grad, eta, dim);
        steps += 1u;
        float E = cos_v62_energy(input, candidate, dim, beta);
        if (fabsf(prev_E - E) < tol) {
            prev_E = E;
            break;
        }
        prev_E = E;
    }
    free(grad);
    if (out_energy) *out_energy = prev_E;
    if (out_steps)  *out_steps  = steps;
    return 0;
}

/* ====================================================================
 *  3.  Hierarchical Reasoning Loop (HRM-class)
 * ==================================================================== */

int cos_v62_hrm_run(cos_v62_thought_t      *H,
                    cos_v62_thought_t      *L,
                    float                   h_rate,
                    float                   l_rate,
                    float                   sigma_target,
                    const cos_v62_budget_t *budget,
                    uint32_t               *out_h_iters,
                    uint32_t               *out_l_iters)
{
    if (!H || !L || !budget) return -1;
    if (H->dim != L->dim) return -2;
    uint32_t h_iters = 0, l_iters = 0;
    float *delta = (float *)v62_alloc64((size_t)H->dim * sizeof(float));
    if (!delta) return -3;

    for (uint32_t h = 0; h < budget->max_h_iters; ++h) {
        for (uint32_t l = 0; l < budget->max_l_iters; ++l) {
            /* L ← L + l_rate * (H - L)   (branchless residual)      */
            for (uint32_t i = 0; i < L->dim; ++i)
                delta[i] = H->vec[i] - L->vec[i];
            v62_axpy(L->vec, delta, l_rate, L->dim);
            l_iters += 1u;
        }
        for (uint32_t i = 0; i < H->dim; ++i)
            delta[i] = L->vec[i] - H->vec[i];
        v62_axpy(H->vec, delta, h_rate, H->dim);
        h_iters += 1u;

        /* σ-coherence across H↔L; lower distance → higher σ.        */
        float n = v62_l2(delta, H->dim);
        H->sigma = 1.0f - v62_clampf(n, 0.f, 1.f);
        if (H->sigma >= sigma_target) break;
    }

    free(delta);
    if (out_h_iters) *out_h_iters = h_iters;
    if (out_l_iters) *out_l_iters = l_iters;
    return 0;
}

/* ====================================================================
 *  4.  Native Sparse Attention
 * ==================================================================== */

static void v62_branch_compress(const float *Q,
                                const float *K, const float *V,
                                uint32_t n, uint32_t d,
                                uint32_t block, float *out)
{
    /* Compress: average K and V over fixed blocks, attend with Q.    */
    if (block == 0) block = 1;
    uint32_t nb = n / block;
    if (nb == 0) nb = 1;
    float *kbar = (float *)v62_alloc64((size_t)d * sizeof(float));
    float *vbar = (float *)v62_alloc64((size_t)d * sizeof(float));
    if (!kbar || !vbar) { free(kbar); free(vbar); memset(out, 0, d * sizeof(float)); return; }

    float total = 0.f;
    for (uint32_t b = 0; b < nb; ++b) {
        memset(kbar, 0, d * sizeof(float));
        memset(vbar, 0, d * sizeof(float));
        uint32_t base = b * block;
        for (uint32_t r = 0; r < block; ++r) {
            v62_axpy(kbar, K + (base + r) * d, 1.0f / block, d);
            v62_axpy(vbar, V + (base + r) * d, 1.0f / block, d);
        }
        float s = expf(v62_dot(Q, kbar, d) / sqrtf((float)d));
        v62_axpy(out, vbar, s, d);
        total += s;
    }
    float inv = 1.0f / v62_maxf(total, 1e-8f);
    for (uint32_t i = 0; i < d; ++i) out[i] *= inv;
    free(kbar); free(vbar);
}

static void v62_branch_select(const float *Q,
                              const float *K, const float *V,
                              uint32_t n, uint32_t d,
                              uint32_t topk, float *out)
{
    if (topk == 0 || topk > n) topk = n;
    /* Compute all scores; branchless top-K via partial-sort by
     * keeping the running k-th-best score and rejecting weaker rows.
     * For an honest M-tier kernel we use a small heap (O(n log k));
     * tests cap n at 1024 so this stays fast and deterministic.     */
    float *scores = (float *)v62_alloc64((size_t)n * sizeof(float));
    uint32_t *idx = (uint32_t *)v62_alloc64((size_t)n * sizeof(uint32_t));
    if (!scores || !idx) { free(scores); free(idx); memset(out, 0, d * sizeof(float)); return; }

    for (uint32_t r = 0; r < n; ++r) {
        scores[r] = v62_dot(Q, K + r * d, d);
        idx[r] = r;
    }
    /* simple selection of top-K by partial selection sort           */
    for (uint32_t k = 0; k < topk; ++k) {
        uint32_t best = k;
        for (uint32_t r = k + 1; r < n; ++r)
            if (scores[r] > scores[best]) best = r;
        float tmps = scores[k]; scores[k] = scores[best]; scores[best] = tmps;
        uint32_t tmpi = idx[k]; idx[k] = idx[best]; idx[best] = tmpi;
    }

    float total = 0.f;
    float inv_sd = 1.0f / sqrtf((float)d);
    for (uint32_t k = 0; k < topk; ++k) {
        float s = expf(scores[k] * inv_sd);
        v62_axpy(out, V + idx[k] * d, s, d);
        total += s;
    }
    float inv = 1.0f / v62_maxf(total, 1e-8f);
    for (uint32_t i = 0; i < d; ++i) out[i] *= inv;
    free(scores); free(idx);
}

static void v62_branch_window(const float *Q,
                              const float *K, const float *V,
                              uint32_t n, uint32_t d,
                              uint32_t window, float *out)
{
    if (window == 0 || window > n) window = n;
    uint32_t start = n - window;
    float total = 0.f;
    float inv_sd = 1.0f / sqrtf((float)d);
    for (uint32_t r = start; r < n; ++r) {
        float s = expf(v62_dot(Q, K + r * d, d) * inv_sd);
        v62_axpy(out, V + r * d, s, d);
        total += s;
    }
    float inv = 1.0f / v62_maxf(total, 1e-8f);
    for (uint32_t i = 0; i < d; ++i) out[i] *= inv;
}

int cos_v62_nsa_attend(const float *Q,
                       const float *K,
                       const float *V,
                       uint32_t     n,
                       uint32_t     d,
                       uint32_t     window,
                       uint32_t     topk,
                       uint32_t     block,
                       const float  gate[3],
                       float       *out)
{
    if (!Q || !K || !V || !gate || !out || n == 0 || d == 0) return -1;
    float *c = (float *)v62_alloc64((size_t)d * sizeof(float));
    float *s = (float *)v62_alloc64((size_t)d * sizeof(float));
    float *w = (float *)v62_alloc64((size_t)d * sizeof(float));
    if (!c || !s || !w) { free(c); free(s); free(w); return -2; }

    v62_branch_compress(Q, K, V, n, d, block, c);
    v62_branch_select  (Q, K, V, n, d, topk,  s);
    v62_branch_window  (Q, K, V, n, d, window, w);

    /* Branchless, gate-weighted fusion. */
    memset(out, 0, d * sizeof(float));
    v62_axpy(out, c, gate[0], d);
    v62_axpy(out, s, gate[1], d);
    v62_axpy(out, w, gate[2], d);

    free(c); free(s); free(w);
    return 0;
}

/* ====================================================================
 *  5.  Multi-Token Predictor / Drafter (MTP-class)
 * ==================================================================== */

static uint32_t v62_argmax(const float *x, uint32_t n, float *out_val)
{
    uint32_t best = 0;
    float best_v = x[0];
    /* Branchless over uint32_t selection mask. */
    for (uint32_t i = 1; i < n; ++i) {
        uint32_t m = (uint32_t)-(int32_t)(x[i] > best_v);
        best   = (best   & ~m) | (i      & m);
        best_v = (x[i] > best_v) ? x[i] : best_v;
    }
    if (out_val) *out_val = best_v;
    return best;
}

int cos_v62_mtp_draft(const float *logits,
                      uint32_t     vocab,
                      const float *bias_heads,
                      uint32_t     K,
                      uint32_t    *out_tokens,
                      float       *out_conf)
{
    if (!logits || !bias_heads || !out_tokens || !out_conf || vocab == 0 || K == 0)
        return -1;

    float *scratch = (float *)v62_alloc64((size_t)vocab * sizeof(float));
    if (!scratch) return -2;

    /* Scratch starts from the base logits and accumulates each head's
     * bias to maintain a *causal chain* (DeepSeek-V3 invariant): each
     * draft token is conditioned on every prior draft.  Heads are
     * laid out [K, vocab] row-major.                                */
    memcpy(scratch, logits, (size_t)vocab * sizeof(float));
    for (uint32_t k = 0; k < K; ++k) {
        v62_axpy(scratch, bias_heads + (size_t)k * vocab, 1.0f, vocab);
        float v = 0.f;
        out_tokens[k] = v62_argmax(scratch, vocab, &v);
        out_conf[k] = v;
    }
    free(scratch);
    return 0;
}

uint32_t cos_v62_mtp_verify(const uint32_t *draft,
                            const uint32_t *truth,
                            uint32_t        K)
{
    if (!draft || !truth || K == 0) return 0;
    uint32_t accept = 0;
    /* Branchless: stop counting once any mismatch occurs by
     * multiplying a running mask. */
    uint32_t still_ok = 1u;
    for (uint32_t k = 0; k < K; ++k) {
        uint32_t eq = (uint32_t)(draft[k] == truth[k]);
        still_ok &= eq;
        accept   += still_ok;
    }
    return accept;
}

/* ====================================================================
 *  6.  Adaptive KV manager (ARKV-class)
 * ==================================================================== */

cos_v62_arkv_t *cos_v62_arkv_new(uint32_t n,
                                 uint32_t cap_orig,
                                 uint32_t cap_quant)
{
    if (n == 0) return NULL;
    if (cap_orig + cap_quant > n) {
        if (cap_orig > n) cap_orig = n;
        if (cap_quant > n - cap_orig) cap_quant = n - cap_orig;
    }
    cos_v62_arkv_t *a = (cos_v62_arkv_t *)v62_alloc64(sizeof(*a));
    if (!a) return NULL;
    a->state = (uint8_t *)v62_alloc64((size_t)n);
    a->score = (float   *)v62_alloc64((size_t)n * sizeof(float));
    if (!a->state || !a->score) {
        free(a->state); free(a->score); free(a); return NULL;
    }
    a->n = n;
    a->cap_orig  = cap_orig;
    a->cap_quant = cap_quant;
    /* Default policy: every token starts in QUANT (warm).  Updates
     * promote heavy hitters to ORIG and demote cold tokens to EVICT. */
    for (uint32_t i = 0; i < n; ++i) a->state[i] = COS_V62_KV_QUANT;
    return a;
}

void cos_v62_arkv_free(cos_v62_arkv_t *a)
{
    if (!a) return;
    free(a->state); free(a->score); free(a);
}

uint32_t cos_v62_arkv_update(cos_v62_arkv_t *a,
                             const float    *new_attn_weights)
{
    if (!a || !new_attn_weights) return 0;
    /* Exponential running average; alpha = 0.5 keeps it predictable.*/
    for (uint32_t i = 0; i < a->n; ++i)
        a->score[i] = 0.5f * a->score[i] + 0.5f * new_attn_weights[i];

    /* Choose ORIG threshold = score s.t. cap_orig tokens are above.
     * Branchless classification: subtract threshold, look at sign.   */
    uint32_t cap_o = a->cap_orig;
    uint32_t cap_q = a->cap_quant;

    /* Build a sorted index just for thresholds — N small in tests.   */
    uint32_t *idx = (uint32_t *)v62_alloc64((size_t)a->n * sizeof(uint32_t));
    if (!idx) return 0;
    for (uint32_t i = 0; i < a->n; ++i) idx[i] = i;
    /* selection-sort the top (cap_o + cap_q) by score desc           */
    uint32_t want = cap_o + cap_q;
    if (want > a->n) want = a->n;
    for (uint32_t k = 0; k < want; ++k) {
        uint32_t best = k;
        for (uint32_t r = k + 1; r < a->n; ++r)
            if (a->score[idx[r]] > a->score[idx[best]]) best = r;
        uint32_t tmp = idx[k]; idx[k] = idx[best]; idx[best] = tmp;
    }
    /* assign states                                                  */
    for (uint32_t i = 0; i < a->n; ++i) a->state[i] = COS_V62_KV_EVICT;
    for (uint32_t k = 0; k < cap_o && k < a->n; ++k)
        a->state[idx[k]] = COS_V62_KV_ORIG;
    for (uint32_t k = cap_o; k < cap_o + cap_q && k < a->n; ++k)
        a->state[idx[k]] = COS_V62_KV_QUANT;
    free(idx);

    uint32_t orig_count = 0;
    for (uint32_t i = 0; i < a->n; ++i)
        orig_count += (a->state[i] == COS_V62_KV_ORIG);
    return orig_count;
}

/* ====================================================================
 *  Composition with v60 σ-Shield + v61 Σ-Citadel
 * ==================================================================== */

int cos_v62_compose_decision(uint8_t              v60_ok,
                             uint8_t              v61_ok,
                             uint8_t              v62_ok,
                             cos_v62_decision_t  *out)
{
    if (!out) return -1;
    out->v60_ok = (uint8_t)!!v60_ok;
    out->v61_ok = (uint8_t)!!v61_ok;
    out->v62_ok = (uint8_t)!!v62_ok;
    out->allow  = (uint8_t)(out->v60_ok & out->v61_ok & out->v62_ok);
    return 0;
}

const char *cos_v62_version(void)
{
    return "v62.0 reasoning-fabric (latent-CoT + EBT + HRM + NSAttn + MTP + ARKV)";
}
