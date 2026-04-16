/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Creation OS v59 — σ-Budget: scoring + branchless online decision +
 * offline trace summary.
 *
 * The decision kernel is branchless on the hot path (.cursorrules
 * item 4).  Per-step scores are reduced via either a scalar prefetching
 * loop or an explicit NEON 4-accumulator SoA path (item 5).  All
 * scratch buffers are 64-byte aligned (item 2) with prefetch 16 lanes
 * ahead (item 3).  No Accelerate, no Metal, no Core ML (item 9).
 */

#include "sigma_budget.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#if defined(__aarch64__) && !defined(COS_V59_NO_NEON)
#  include <arm_neon.h>
#  define COS_V59_HAVE_NEON 1
#else
#  define COS_V59_HAVE_NEON 0
#endif

/* ------------------------------------------------------------------ */
/* Defaults                                                            */
/* ------------------------------------------------------------------ */

void cos_v59_policy_default(cos_v59_policy_t *out)
{
    if (!out) return;
    out->budget_max_steps = 32;
    out->budget_min_steps = 2;
    out->history_window   = 4;

    out->alpha_epistemic  = 1.00f;  /* ε penalises readiness (more ε → further from exit) */
    out->beta_stability   = 0.75f;  /* stable answer drives EXIT       */
    out->gamma_reflection = 0.25f;  /* reflection tag nudges EXIT      */
    out->delta_aleatoric  = 1.50f;  /* α penalises readiness too       */

    out->tau_exit         =  0.50f; /* readiness ≥ 0.50 → EARLY_EXIT   */
    out->tau_expand       =  0.00f; /* reserved; EXPAND is σ-gated     */
    out->alpha_dominance  =  0.65f; /* α/(ε+α) ≥ 0.65 → ABSTAIN-eligible */
    out->sigma_high       =  1.00f; /* ε + α ≥ 1.00  → high-σ regime    */
}

cos_v59_version_t cos_v59_version(void)
{
    cos_v59_version_t v;
    v.major = 59;
    v.minor = 0;
    v.patch = 1;
    return v;
}

const char *cos_v59_decision_tag(uint8_t d)
{
    switch (d) {
        case COS_V59_CONTINUE:   return "CONTINUE";
        case COS_V59_EARLY_EXIT: return "EARLY_EXIT";
        case COS_V59_EXPAND:     return "EXPAND";
        case COS_V59_ABSTAIN:    return "ABSTAIN";
        default:                 return "?";
    }
}

/* ------------------------------------------------------------------ */
/* Scoring                                                             */
/* ------------------------------------------------------------------ */

/* Readiness score (high = ready to exit):
 *
 *   stability_term   =  β · (1 − answer_delta)   (stable answer → +)
 *   reflection_term  =  γ · reflection_signal    (reflection tag → +)
 *   epistemic_term   = −α_ε · epistemic          (more ε → further from exit)
 *   aleatoric_term   = −δ   · aleatoric          (more α → further from exit)
 *
 *   score = stability + reflection − α_ε·ε − δ·α
 *
 * High score = crystallising answer, low σ, exit-candidate.  The
 * decision kernel uses (ε+α) and α/(ε+α) separately to split EXPAND
 * from ABSTAIN in the high-σ regime; readiness is only one of four
 * inputs. */
float cos_v59_score_step(const cos_v59_step_t   *s,
                         const cos_v59_policy_t *p)
{
    if (!s || !p) return 0.0f;
    float stability_term  = p->beta_stability   * (1.0f - s->answer_delta);
    float reflection_term = p->gamma_reflection * s->reflection_signal;
    float epistemic_term  = p->alpha_epistemic  * s->epistemic;
    float aleatoric_term  = p->delta_aleatoric  * s->aleatoric;
    return stability_term + reflection_term
         - epistemic_term - aleatoric_term;
}

void cos_v59_score_batch(const cos_v59_step_t  *steps,
                         int32_t                n,
                         const cos_v59_policy_t *p,
                         float                 *scores_out)
{
    if (!steps || !p || !scores_out || n <= 0) return;
    for (int32_t i = 0; i < n; ++i) {
        if (i + 16 < n) __builtin_prefetch(&steps[i + 16], 0, 3);
        scores_out[i] = cos_v59_score_step(&steps[i], p);
    }
}

/* Explicit NEON 4-accumulator SoA reduction.  Four s0..s3 lanes, four
 * parallel `vfmaq_f32` stages — the exact .cursorrules item 5 pattern.
 * Writes into `scores_out[i]` for `i` aligned to 16.  Tail handled
 * scalar. */
void cos_v59_score_soa_neon(const float *epistemic,
                            const float *aleatoric,
                            const float *stability,
                            const float *reflection,
                            int32_t      n,
                            float        alpha,
                            float        beta,
                            float        gamma,
                            float        delta,
                            float       *scores_out)
{
    if (!epistemic || !aleatoric || !stability || !reflection ||
        !scores_out || n <= 0) return;

#if COS_V59_HAVE_NEON
    float32x4_t va =  vdupq_n_f32(-alpha);  /* ε subtracts from readiness */
    float32x4_t vb =  vdupq_n_f32(beta);
    float32x4_t vg =  vdupq_n_f32(gamma);
    float32x4_t vd =  vdupq_n_f32(-delta);  /* α subtracts from readiness */

    int32_t i = 0;
    for (; i + 16 <= n; i += 16) {
        if (i + 64 < n) {
            __builtin_prefetch(&epistemic[i + 64],  0, 3);
            __builtin_prefetch(&aleatoric[i + 64],  0, 3);
            __builtin_prefetch(&stability[i + 64],  0, 3);
            __builtin_prefetch(&reflection[i + 64], 0, 3);
        }

        float32x4_t e0 = vld1q_f32(&epistemic[i +  0]);
        float32x4_t e1 = vld1q_f32(&epistemic[i +  4]);
        float32x4_t e2 = vld1q_f32(&epistemic[i +  8]);
        float32x4_t e3 = vld1q_f32(&epistemic[i + 12]);

        float32x4_t a0 = vld1q_f32(&aleatoric[i +  0]);
        float32x4_t a1 = vld1q_f32(&aleatoric[i +  4]);
        float32x4_t a2 = vld1q_f32(&aleatoric[i +  8]);
        float32x4_t a3 = vld1q_f32(&aleatoric[i + 12]);

        float32x4_t st0 = vld1q_f32(&stability[i +  0]);
        float32x4_t st1 = vld1q_f32(&stability[i +  4]);
        float32x4_t st2 = vld1q_f32(&stability[i +  8]);
        float32x4_t st3 = vld1q_f32(&stability[i + 12]);

        float32x4_t rf0 = vld1q_f32(&reflection[i +  0]);
        float32x4_t rf1 = vld1q_f32(&reflection[i +  4]);
        float32x4_t rf2 = vld1q_f32(&reflection[i +  8]);
        float32x4_t rf3 = vld1q_f32(&reflection[i + 12]);

        /* Four parallel accumulators, three FMA stages each. */
        float32x4_t s0 = vmulq_f32(vb, st0);
        float32x4_t s1 = vmulq_f32(vb, st1);
        float32x4_t s2 = vmulq_f32(vb, st2);
        float32x4_t s3 = vmulq_f32(vb, st3);

        s0 = vfmaq_f32(s0, vg, rf0);
        s1 = vfmaq_f32(s1, vg, rf1);
        s2 = vfmaq_f32(s2, vg, rf2);
        s3 = vfmaq_f32(s3, vg, rf3);

        s0 = vfmaq_f32(s0, va, e0);
        s1 = vfmaq_f32(s1, va, e1);
        s2 = vfmaq_f32(s2, va, e2);
        s3 = vfmaq_f32(s3, va, e3);

        s0 = vfmaq_f32(s0, vd, a0);
        s1 = vfmaq_f32(s1, vd, a1);
        s2 = vfmaq_f32(s2, vd, a2);
        s3 = vfmaq_f32(s3, vd, a3);

        vst1q_f32(&scores_out[i +  0], s0);
        vst1q_f32(&scores_out[i +  4], s1);
        vst1q_f32(&scores_out[i +  8], s2);
        vst1q_f32(&scores_out[i + 12], s3);
    }
    for (; i < n; ++i) {
        scores_out[i] =  beta  * stability[i]
                      +  gamma * reflection[i]
                      -  alpha * epistemic[i]
                      -  delta * aleatoric[i];
    }
#else
    for (int32_t i = 0; i < n; ++i) {
        scores_out[i] =  beta  * stability[i]
                      +  gamma * reflection[i]
                      -  alpha * epistemic[i]
                      -  delta * aleatoric[i];
    }
#endif
}

/* ------------------------------------------------------------------ */
/* Branchless online decision                                          */
/* ------------------------------------------------------------------ */

/* Compute ε + α and α/(ε+α) branchlessly.  α/(ε+α) is undefined when
 * ε+α = 0; we return 0 in that case via a sentinel-add.  The sentinel
 * is tiny (1e-12f) and leaks at most 1e-12f into the dominance ratio;
 * the ABSTAIN mask threshold is 0.65 by default so the leak never
 * flips a decision. */
static inline float v59_sigma_total(const cos_v59_step_t *s)
{
    return s->epistemic + s->aleatoric;
}

static inline float v59_alpha_frac(const cos_v59_step_t *s)
{
    float tot = v59_sigma_total(s) + 1.0e-12f;
    return s->aleatoric / tot;
}

int cos_v59_decide_online(const cos_v59_step_t   *steps,
                          int32_t                 n,
                          const cos_v59_policy_t *p,
                          uint8_t                *decision_out)
{
    if (!steps || !p || !decision_out || n < 1) return -1;

    const cos_v59_step_t *cur = &steps[n - 1];

    float score      = cos_v59_score_step(cur, p);
    float sigma_tot  = v59_sigma_total(cur);
    float alpha_frac = v59_alpha_frac(cur);

    /* 0/1 lane masks.  All comparisons emit 0 or 1; ORed with
     * bitwise & | ~ to layer decisions without branching. */
    int above_min    = (n >= p->budget_min_steps);
    int at_cap       = (n >= p->budget_max_steps);
    int ready        = (score >= p->tau_exit);
    int sigma_high   = (sigma_tot >= p->sigma_high);
    int alpha_dom    = (alpha_frac >= p->alpha_dominance);

    /* Layered, priority-ordered masks:
     *   1. at_cap                       → EARLY_EXIT (ceiling)
     *   2. sigma_high & alpha_dom &>min → ABSTAIN    (irreducible)
     *   3. sigma_high & ¬alpha_dom      → EXPAND     (reducible)
     *   4. ready & above_min            → EARLY_EXIT (crystallised)
     *   5. default                      → CONTINUE
     */
    int m_exit_cap   =  at_cap;
    int m_abstain    = (~m_exit_cap) & sigma_high & alpha_dom & above_min;
    int m_expand     = (~m_exit_cap) & (~m_abstain) & sigma_high
                                    & (~alpha_dom);
    int m_exit_ready = (~m_exit_cap) & (~m_abstain) & (~m_expand)
                                    & ready & above_min;
    int m_exit       =  m_exit_cap | m_exit_ready;
    int m_continue   = (~m_exit) & (~m_abstain) & (~m_expand) & 1;

    uint8_t d = (uint8_t)( (COS_V59_EARLY_EXIT * m_exit)
                         | (COS_V59_ABSTAIN    * m_abstain)
                         | (COS_V59_EXPAND     * m_expand)
                         | (COS_V59_CONTINUE   * m_continue) );

    *decision_out = d;
    return 0;
}

/* ------------------------------------------------------------------ */
/* Offline trace summary                                               */
/* ------------------------------------------------------------------ */

int cos_v59_summarize_trace(const cos_v59_step_t   *steps,
                            int32_t                 n,
                            const cos_v59_policy_t *p,
                            cos_v59_trace_summary_t *summary_out)
{
    if (!p || !summary_out || n < 0) return -1;
    if (n > 0 && !steps) return -1;

    memset(summary_out, 0, sizeof(*summary_out));
    if (n == 0) return 0;

    float last_score = 0.0f;
    uint8_t last_decision = 0;

    for (int32_t i = 0; i < n; ++i) {
        if (i + 16 < n) __builtin_prefetch(&steps[i + 16], 0, 3);
        summary_out->total_epistemic  += steps[i].epistemic;
        summary_out->total_aleatoric  += steps[i].aleatoric;

        uint8_t d = 0;
        /* Replay the online decision so summary.final_decision matches
         * what an online caller would have seen at the final step. */
        cos_v59_decide_online(steps, i + 1, p, &d);
        last_decision = d;
        last_score    = cos_v59_score_step(&steps[i], p);

        /* Branchless histogram increment: write all four, the right
         * one is +1 and the other three are +0.  Mask logic identical
         * to the decision kernel. */
        summary_out->continues    += (d == COS_V59_CONTINUE);
        summary_out->early_exits  += (d == COS_V59_EARLY_EXIT);
        summary_out->expansions   += (d == COS_V59_EXPAND);
        summary_out->abstentions  += (d == COS_V59_ABSTAIN);
    }
    summary_out->n_steps         = n;
    summary_out->final_step_score = last_score;
    summary_out->final_decision   = last_decision;
    return 0;
}

/* ------------------------------------------------------------------ */
/* Aligned allocator                                                   */
/* ------------------------------------------------------------------ */

cos_v59_step_t *cos_v59_alloc_steps(int32_t n)
{
    if (n <= 0) return NULL;
    size_t bytes = (size_t)n * sizeof(cos_v59_step_t);
    size_t aligned = (bytes + 63u) & ~(size_t)63u;
    void *m = aligned_alloc(64, aligned);
    if (!m) return NULL;
    memset(m, 0, aligned);
    return (cos_v59_step_t *)m;
}
