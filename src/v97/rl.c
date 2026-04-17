/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Creation OS v97 — σ-RL implementation.
 */

#include "rl.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

static inline cos_v97_q16_t q_mul(cos_v97_q16_t a, cos_v97_q16_t b)
{
    int64_t p = (int64_t)a * (int64_t)b;
    return (cos_v97_q16_t)(p >> 16);
}

static inline cos_v97_q16_t q_clamp(cos_v97_q16_t x,
                                    cos_v97_q16_t lo,
                                    cos_v97_q16_t hi)
{
    /* Branchless-ish clamp via cmov-friendly min/max.  The compiler
     * lowers the ternaries to conditional-select on ARM and CMOV on
     * x86; no taken branches. */
    cos_v97_q16_t y = (x < lo) ? lo : x;
    y              = (y > hi) ? hi : y;
    return y;
}

void cos_v97_agent_init(cos_v97_agent_t *a, uint64_t seed)
{
    (void)seed;                                   /* init is data-free */
    memset(a, 0, sizeof(*a));
    a->sentinel = COS_V97_SENTINEL;
}

/* Bellman update:  Q[s,a] ← Q[s,a] + α · (r + γ·max_a' Q[s',a']·(1−done) − Q[s,a]) */
void cos_v97_q_update(cos_v97_agent_t *a,
                      uint32_t s, uint32_t act,
                      cos_v97_q16_t reward,
                      uint32_t s_next,
                      uint32_t done)
{
    if (s >= COS_V97_NSTATES || act >= COS_V97_NACTIONS) return;

    cos_v97_q16_t q_cur    = a->Q[s][act];
    cos_v97_q16_t v_next   = cos_v97_value(a, (s_next < COS_V97_NSTATES) ? s_next : 0u);
    uint32_t      cont     = (done != 0u) ? 0u : 1u;
    cos_v97_q16_t boot     = (cos_v97_q16_t)(cont) * 0;           /* unused */
    (void)boot;

    /* target = r + γ · V(s') · (1 − done) */
    cos_v97_q16_t gamma_v  = q_mul((cos_v97_q16_t)COS_V97_GAMMA, v_next);
    cos_v97_q16_t target   = (cos_v97_q16_t)(reward + (cos_v97_q16_t)(cont * (uint32_t)gamma_v));

    cos_v97_q16_t td       = (cos_v97_q16_t)(target - q_cur);
    /* Arithmetic right-shift on signed types is implementation-defined
     * in C11 but all supported targets (gcc/clang on ARM64/x86_64) do
     * it sign-extended.  For fully portable truncation-towards-zero we
     * divide instead. */
    cos_v97_q16_t delta    = (cos_v97_q16_t)(td / (int32_t)(1u << COS_V97_ALPHA_RSHIFT));
    cos_v97_q16_t q_new    = (cos_v97_q16_t)(q_cur + delta);
    q_new = q_clamp(q_new, (cos_v97_q16_t)(-COS_V97_Q_CLAMP),
                            (cos_v97_q16_t)( COS_V97_Q_CLAMP));
    if (q_new == (cos_v97_q16_t)(COS_V97_Q_CLAMP) ||
        q_new == (cos_v97_q16_t)(-COS_V97_Q_CLAMP)) {
        /* We touched the clamp: not inherently an error but tracked. */
        ++a->clamp_violations;
    }
    a->Q[s][act] = q_new;
    ++a->updates;
}

uint32_t cos_v97_greedy(const cos_v97_agent_t *a, uint32_t s)
{
    if (s >= COS_V97_NSTATES) return 0u;
    uint32_t      best_i = 0u;
    cos_v97_q16_t best_v = a->Q[s][0];
    for (uint32_t i = 1u; i < COS_V97_NACTIONS; ++i) {
        cos_v97_q16_t v = a->Q[s][i];
        uint32_t      is_better = (v > best_v) ? 1u : 0u;
        best_v = is_better ? v : best_v;
        best_i = is_better ? i : best_i;
    }
    return best_i;
}

cos_v97_q16_t cos_v97_value(const cos_v97_agent_t *a, uint32_t s)
{
    if (s >= COS_V97_NSTATES) return 0;
    cos_v97_q16_t v = a->Q[s][0];
    for (uint32_t i = 1u; i < COS_V97_NACTIONS; ++i) {
        cos_v97_q16_t x = a->Q[s][i];
        v = (x > v) ? x : v;
    }
    return v;
}

void cos_v97_gae(const cos_v97_q16_t *rewards,
                 const cos_v97_q16_t *values,
                 uint32_t T,
                 cos_v97_q16_t *A)
{
    if (T == 0u) return;
    cos_v97_q16_t gl = q_mul((cos_v97_q16_t)COS_V97_GAMMA,
                             (cos_v97_q16_t)COS_V97_LAMBDA);

    /* Reverse scan: A_T-1 ← δ_T-1; then A_t ← δ_t + γλ·A_{t+1}. */
    cos_v97_q16_t acc = 0;
    for (int32_t t = (int32_t)T - 1; t >= 0; --t) {
        cos_v97_q16_t gamma_v_next = q_mul((cos_v97_q16_t)COS_V97_GAMMA,
                                           values[t + 1]);
        cos_v97_q16_t delta = (cos_v97_q16_t)((int32_t)rewards[t] +
                                              (int32_t)gamma_v_next -
                                              (int32_t)values[t]);
        acc = (cos_v97_q16_t)((int32_t)delta + (int32_t)q_mul(gl, acc));
        A[t] = acc;
    }
}

/* PPO-clip surrogate:
 *     L_clip = min( ρ·A,  clip(ρ, 1−ε, 1+ε)·A )    if A ≥ 0
 *            = max( ρ·A,  clip(ρ, 1−ε, 1+ε)·A )    if A <  0
 *
 * Equivalently: L_clip = ρ·A saturated above at (1+ε)·A (resp. below
 * at (1−ε)·A for A < 0), so the gradient is zero once the policy
 * exceeds the trust region on the side that hurts training. */
cos_v97_q16_t cos_v97_ppo_clip(cos_v97_q16_t ratio, cos_v97_q16_t A)
{
    cos_v97_q16_t lo = (cos_v97_q16_t)(COS_V97_Q1 - COS_V97_CLIP_EPS);
    cos_v97_q16_t hi = (cos_v97_q16_t)(COS_V97_Q1 + COS_V97_CLIP_EPS);
    cos_v97_q16_t r_clip = q_clamp(ratio, lo, hi);
    cos_v97_q16_t unclipped = q_mul(ratio,  A);
    cos_v97_q16_t clipped   = q_mul(r_clip, A);
    /* Canonical Schulman-2017 PPO-clip: L = min(ρ·A, clip(ρ, 1−ε, 1+ε)·A)
     * for both signs of A.  The branch below is a data-independent
     * pointer-free min — compilers lower it to a single CSEL on ARM64. */
    return (unclipped < clipped) ? unclipped : clipped;
}

uint32_t cos_v97_ok(const cos_v97_agent_t *a)
{
    uint32_t sent = (a->sentinel == COS_V97_SENTINEL) ? 1u : 0u;
    uint32_t bounded = 1u;
    for (uint32_t s = 0u; s < COS_V97_NSTATES; ++s) {
        for (uint32_t i = 0u; i < COS_V97_NACTIONS; ++i) {
            int32_t q = (int32_t)a->Q[s][i];
            if (q >  (int32_t)COS_V97_Q_CLAMP) bounded = 0u;
            if (q < -(int32_t)COS_V97_Q_CLAMP) bounded = 0u;
        }
    }
    return sent & bounded;
}

uint32_t cos_v97_compose_decision(uint32_t v96_composed_ok, uint32_t v97_ok)
{
    return v96_composed_ok & v97_ok;
}
