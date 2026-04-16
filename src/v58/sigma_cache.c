/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Creation OS v58 — σ-Cache: scoring, branchless decision, compaction.
 *
 * The decision kernel is deliberately written without `if` on the
 * hot path (per .cursorrules item 4: branchless kernel).  Decision
 * masks are derived by float comparisons that produce 0/1 ints,
 * combined with bitwise ops, then translated into the four-valued
 * decision byte by an additive branchless mux.  Sink protection is
 * applied last as an OR over the KEEP_FULL bit.
 *
 * Threshold selection uses a deterministic copy + qsort_r-free
 * descending sort to find the K-th largest score where K is the
 * remaining capacity after sinks.  The chosen threshold is exposed
 * in the summary so callers can audit the policy externally.
 *
 * No allocation on the hot path.  `cos_v58_decide` allocates one
 * float scratch array of length n via `aligned_alloc` (64-byte) and
 * frees it before returning; this is the only deviation from the
 * "zero-alloc" rule and is necessary for the deterministic threshold
 * selection.  Callers that already hold a scratch buffer can use the
 * separate `cos_v58_score_batch` API and run their own selection.
 */

#include "sigma_cache.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#if defined(__aarch64__) && !defined(COS_V58_NO_NEON)
#  include <arm_neon.h>
#  define COS_V58_HAVE_NEON 1
#else
#  define COS_V58_HAVE_NEON 0
#endif

/* ------------------------------------------------------------------ */
/* Defaults                                                            */
/* ------------------------------------------------------------------ */

void cos_v58_policy_default(cos_v58_policy_t *out)
{
    if (!out) return;
    out->capacity         = 512;
    out->sink_count       = 4;
    out->recency_window   = 64;

    out->alpha_epistemic  = 1.00f;   /* primary driver — v34 epistemic */
    out->beta_attention   = 0.50f;   /* secondary — Heavy-Hitter prior */
    out->gamma_recency    = 0.25f;   /* tertiary — StreamingLLM tail */
    out->delta_aleatoric  = 0.40f;   /* penalty on noise carriers */

    out->tau_full         = 0.75f;
    out->tau_int8         = 0.25f;
}

cos_v58_version_t cos_v58_version(void)
{
    cos_v58_version_t v;
    v.major = 58;
    v.minor = 0;
    v.patch = 1;
    return v;
}

const char *cos_v58_decision_tag(uint8_t d)
{
    switch (d) {
        case COS_V58_KEEP_FULL: return "FULL";
        case COS_V58_KEEP_INT8: return "INT8";
        case COS_V58_KEEP_INT4: return "INT4";
        case COS_V58_EVICTED:   return "EVICT";
        default:                return "?";
    }
}

/* ------------------------------------------------------------------ */
/* Scoring                                                             */
/* ------------------------------------------------------------------ */

/* Branchless recency bonus: 1.0 when the token is in the recent
 * window (age in [0, recency_window]), 0.0 otherwise.  Computed
 * without if-statements via two bool→int casts ANDed together. */
static inline float v58_recency_bonus(int32_t current_pos,
                                      int32_t seq_pos,
                                      int32_t recency_window)
{
    int64_t age   = (int64_t)current_pos - (int64_t)seq_pos;
    int     ge0   = (age >= 0);
    int     lewin = (age <= (int64_t)recency_window);
    int     in    = ge0 & lewin;
    return (float)in;
}

float cos_v58_score_token(const cos_v58_token_t *tok,
                          int32_t                current_pos,
                          const cos_v58_policy_t *p)
{
    if (!tok || !p) return 0.0f;

    float bonus = v58_recency_bonus(current_pos, tok->seq_pos,
                                    p->recency_window);

    float s = p->alpha_epistemic * tok->epistemic_contrib
            + p->beta_attention  * tok->attention_mass
            + p->gamma_recency   * bonus
            - p->delta_aleatoric * tok->aleatoric_signal;

    /* Sinks always score above tau_full; this is enforced again in
     * the decision kernel as an OR mask, but baking it into the score
     * makes external diagnostics (microbench, debug tooling) easier
     * to read. */
    float sink_lift = (float)tok->is_sink * 1.0e6f;
    return s + sink_lift;
}

void cos_v58_score_batch(const cos_v58_token_t  *tokens,
                         int32_t                 n,
                         int32_t                 current_pos,
                         const cos_v58_policy_t *p,
                         float                  *scores_out)
{
    if (!tokens || !p || !scores_out || n <= 0) return;

    /* The AArch64 autovectoriser at -O2 -march=native lowers this
     * scalar loop to two-wide NEON FMA chains.  An explicit SoA
     * NEON path lives in cos_v58_score_soa_neon() (used by the
     * microbench) for the published 4-accumulator pattern. */
    for (int32_t i = 0; i < n; ++i) {
        /* Prefetch a few cache lines ahead (.cursorrules item 3). */
        if (i + 16 < n) __builtin_prefetch(&tokens[i + 16], 0, 3);

        scores_out[i] = cos_v58_score_token(&tokens[i], current_pos, p);
    }
}

/* Explicit NEON 4-accumulator score reduction over Structure-of-Arrays
 * inputs.  Used by the microbench to exercise the .cursorrules item 5
 * pattern (four parallel sum lanes) on a layout the inference loop
 * would produce directly.  This path is purely additive — it does not
 * apply the sink lift or aleatoric penalty (the microbench compares
 * it against a scalar SoA reference, not against the AoS scorer). */
void cos_v58_score_soa_neon(const float *epistemic,
                            const float *attention,
                            const float *recency_bonus,
                            int32_t      n,
                            float        alpha,
                            float        beta,
                            float        gamma,
                            float       *scores_out)
{
    if (!epistemic || !attention || !recency_bonus || !scores_out || n <= 0)
        return;

#if COS_V58_HAVE_NEON
    float32x4_t va = vdupq_n_f32(alpha);
    float32x4_t vb = vdupq_n_f32(beta);
    float32x4_t vg = vdupq_n_f32(gamma);

    int32_t i = 0;
    for (; i + 16 <= n; i += 16) {
        if (i + 64 < n) {
            __builtin_prefetch(&epistemic[i + 64], 0, 3);
            __builtin_prefetch(&attention[i + 64], 0, 3);
            __builtin_prefetch(&recency_bonus[i + 64], 0, 3);
        }

        float32x4_t e0 = vld1q_f32(&epistemic[i +  0]);
        float32x4_t e1 = vld1q_f32(&epistemic[i +  4]);
        float32x4_t e2 = vld1q_f32(&epistemic[i +  8]);
        float32x4_t e3 = vld1q_f32(&epistemic[i + 12]);

        float32x4_t a0 = vld1q_f32(&attention[i +  0]);
        float32x4_t a1 = vld1q_f32(&attention[i +  4]);
        float32x4_t a2 = vld1q_f32(&attention[i +  8]);
        float32x4_t a3 = vld1q_f32(&attention[i + 12]);

        float32x4_t r0 = vld1q_f32(&recency_bonus[i +  0]);
        float32x4_t r1 = vld1q_f32(&recency_bonus[i +  4]);
        float32x4_t r2 = vld1q_f32(&recency_bonus[i +  8]);
        float32x4_t r3 = vld1q_f32(&recency_bonus[i + 12]);

        float32x4_t s0 = vmulq_f32(va, e0);
        float32x4_t s1 = vmulq_f32(va, e1);
        float32x4_t s2 = vmulq_f32(va, e2);
        float32x4_t s3 = vmulq_f32(va, e3);

        s0 = vfmaq_f32(s0, vb, a0);
        s1 = vfmaq_f32(s1, vb, a1);
        s2 = vfmaq_f32(s2, vb, a2);
        s3 = vfmaq_f32(s3, vb, a3);

        s0 = vfmaq_f32(s0, vg, r0);
        s1 = vfmaq_f32(s1, vg, r1);
        s2 = vfmaq_f32(s2, vg, r2);
        s3 = vfmaq_f32(s3, vg, r3);

        vst1q_f32(&scores_out[i +  0], s0);
        vst1q_f32(&scores_out[i +  4], s1);
        vst1q_f32(&scores_out[i +  8], s2);
        vst1q_f32(&scores_out[i + 12], s3);
    }
    for (; i < n; ++i) {
        scores_out[i] = alpha * epistemic[i]
                      + beta  * attention[i]
                      + gamma * recency_bonus[i];
    }
#else
    for (int32_t i = 0; i < n; ++i) {
        scores_out[i] = alpha * epistemic[i]
                      + beta  * attention[i]
                      + gamma * recency_bonus[i];
    }
#endif
}

/* ------------------------------------------------------------------ */
/* Threshold selection                                                 */
/* ------------------------------------------------------------------ */

static int v58_cmp_desc(const void *a, const void *b)
{
    float fa = *(const float *)a;
    float fb = *(const float *)b;
    /* Descending: positive when fa < fb. */
    return (fa < fb) - (fa > fb);
}

/* Pick the K-th-largest score from `scratch` (which the caller may
 * overwrite).  K is 1-based.
 *   - K <= 0  → +INFINITY   (budget is 0, nothing passes the gate)
 *   - K  > n  → -INFINITY   (budget exceeds supply, everyone passes)
 *   - else    → scratch[K-1] after descending sort. */
static float v58_kth_largest(float *scratch, int32_t n, int32_t k)
{
    if (k <= 0) return  1.0e30f;
    if (k > n)  return -1.0e30f;
    qsort(scratch, (size_t)n, sizeof(float), v58_cmp_desc);
    return scratch[k - 1];
}

/* ------------------------------------------------------------------ */
/* Branchless decision kernel                                          */
/* ------------------------------------------------------------------ */

int cos_v58_decide(const cos_v58_token_t      *tokens,
                   int32_t                     n,
                   int32_t                     current_pos,
                   const cos_v58_policy_t     *p,
                   uint8_t                    *decisions_out,
                   cos_v58_decision_summary_t *summary_out)
{
    if (!p || !summary_out || n < 0)
        return -1;
    if (n > 0 && (!tokens || !decisions_out))
        return -1;

    memset(summary_out, 0, sizeof(*summary_out));
    summary_out->keep_threshold = 1.0e30f;

    if (n == 0) return 0;

    /* Step 1: score every token. */
    size_t bytes = (size_t)n * sizeof(float);
    /* Round up to 64 for aligned_alloc (.cursorrules item 2). */
    size_t aligned_bytes = (bytes + 63u) & ~(size_t)63u;
    float *scores  = (float *)aligned_alloc(64, aligned_bytes);
    float *scratch = (float *)aligned_alloc(64, aligned_bytes);
    if (!scores || !scratch) {
        free(scores);
        free(scratch);
        return -1;
    }

    cos_v58_score_batch(tokens, n, current_pos, p, scores);

    /* Step 2: find the eviction threshold from the non-sink slice.
     * Sinks are unconditionally KEEP_FULL, so the "remaining" budget
     * after sinks is what the threshold must shape.  We copy scores
     * into `scratch` and zero out sink positions there so the sort
     * doesn't double-count them. */
    int32_t sink_protected = 0;
    for (int32_t i = 0; i < n; ++i) {
        scratch[i] = scores[i];
        sink_protected += (int32_t)tokens[i].is_sink;
    }

    int32_t budget = p->capacity - sink_protected;
    if (budget < 0) budget = 0;

    /* Set sink scores to -infinity in the scratch copy so they don't
     * influence the threshold choice. */
    for (int32_t i = 0; i < n; ++i) {
        if (tokens[i].is_sink) scratch[i] = -1.0e30f;
    }

    int32_t non_sink_n = n - sink_protected;
    if (budget > non_sink_n) budget = non_sink_n;
    float threshold = v58_kth_largest(scratch, n, budget);

    /* Step 3: branchless decision write. */
    int32_t kept_full = 0, kept_int8 = 0, kept_int4 = 0, evicted = 0;
    for (int32_t i = 0; i < n; ++i) {
        if (i + 16 < n) __builtin_prefetch(&tokens[i + 16], 0, 3);

        float s = scores[i];

        /* Bool→int (0/1) lanes; no branches. */
        int sink   = (int)tokens[i].is_sink;
        int g_full = (s >= p->tau_full);
        int g_int8 = (s >= p->tau_int8);
        int g_keep = (s >= threshold);

        /* Layered masks (mutually exclusive after AND-NOT cascade):
         *   - m_full : sink OR (kept by budget AND score reaches τ_full)
         *   - m_int8 : kept by budget AND score reaches τ_int8 AND not full
         *   - m_int4 : kept by budget AND not int8 / full
         *   - m_evct : everything else (the inverse of any keep mask)
         *
         * Precision tiers gate on `g_keep` so a high-τ_int8 score that
         * falls below the budget threshold is still EVICTED.  Only the
         * sink bit overrides the budget. */
        int m_full =  sink | (g_keep & g_full);
        int m_int8 = (~m_full) & g_keep & g_int8;
        int m_int4 = (~m_full) & (~m_int8) & g_keep;
        int m_evct = (~m_full) & (~m_int8) & (~m_int4) & 1;

        /* Branchless mux into the four-valued tag. */
        uint8_t d = (uint8_t)( (COS_V58_KEEP_FULL * m_full)
                             | (COS_V58_KEEP_INT8 * m_int8)
                             | (COS_V58_KEEP_INT4 * m_int4)
                             | (COS_V58_EVICTED   * m_evct) );

        decisions_out[i] = d;

        kept_full += m_full;
        kept_int8 += m_int8;
        kept_int4 += m_int4;
        evicted   += m_evct;
    }

    /* Step 4: populate summary. */
    summary_out->kept_full       = kept_full;
    summary_out->kept_int8       = kept_int8;
    summary_out->kept_int4       = kept_int4;
    summary_out->evicted         = evicted;
    summary_out->sink_protected  = sink_protected;
    summary_out->kept_total      = kept_full + kept_int8 + kept_int4;
    summary_out->keep_threshold  = threshold;

    free(scores);
    free(scratch);
    return summary_out->kept_total;
}

/* ------------------------------------------------------------------ */
/* Branchless compaction                                               */
/* ------------------------------------------------------------------ */

int cos_v58_compact(const uint8_t *decisions,
                    int32_t        n,
                    int32_t       *kept_indices_out)
{
    if (!decisions || !kept_indices_out || n < 0) return -1;
    int32_t w = 0;
    for (int32_t i = 0; i < n; ++i) {
        /* Branchless: write i unconditionally, advance w only if kept. */
        kept_indices_out[w] = i;
        int kept = (decisions[i] != COS_V58_EVICTED);
        w += kept;
    }
    return w;
}

/* ------------------------------------------------------------------ */
/* Allocation helpers                                                  */
/* ------------------------------------------------------------------ */

cos_v58_token_t *cos_v58_alloc_tokens(int32_t n)
{
    if (n <= 0) return NULL;
    size_t bytes = (size_t)n * sizeof(cos_v58_token_t);
    size_t aligned_bytes = (bytes + 63u) & ~(size_t)63u;
    void *p = aligned_alloc(64, aligned_bytes);
    if (!p) return NULL;
    memset(p, 0, aligned_bytes);
    return (cos_v58_token_t *)p;
}
