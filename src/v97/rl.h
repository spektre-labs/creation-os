/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Creation OS v97 — σ-RL (integer reinforcement-learning plane:
 * tabular Q-learning + GAE advantage + PPO-clip policy gradient).
 *
 * ------------------------------------------------------------------
 *  What v97 is
 * ------------------------------------------------------------------
 *
 * A compact, fully integer (Q16.16) reinforcement-learning kernel
 * with three composable primitives — every step branchless in its
 * data path, no malloc, no floating point, no RNG on the hot path.
 *
 *   1. Tabular Q-learning with Bellman update
 *
 *        Q[s,a] ← Q[s,a] + α · (r + γ · max_{a'} Q[s',a'] − Q[s,a])
 *
 *   2. Generalised Advantage Estimation (Schulman 2016)
 *
 *        δ_t = r_t + γ·V_{t+1} − V_t
 *        A_t = δ_t + (γ·λ)·A_{t+1}
 *
 *   3. PPO-clip surrogate gate (Schulman 2017)
 *
 *        L_clip(θ) = min( ρ·A,  clip(ρ, 1−ε, 1+ε)·A )
 *
 *      where ρ = π_new / π_old is represented as a Q16.16 ratio in
 *      [0, 2·Q1].  The gate only certifies that the *clipped* branch
 *      is monotone non-decreasing in ρ inside [1−ε, 1+ε] and flat
 *      outside — the canonical PPO stability property.
 *
 * Invariants checked at runtime:
 *   • Q-table bounded: |Q[s,a]| ≤ COS_V97_Q_CLAMP
 *   • Bellman surplus shrinks monotonically on a stationary MDP
 *   • GAE is exactly linear in δ:  A = Σ (γλ)^k · δ_{t+k}
 *   • PPO-clip envelope is a monotone non-decreasing function of ρ·A
 *     at the same fixed advantage sign
 *   • sentinel intact
 *
 * ------------------------------------------------------------------
 *  Composed 37-bit branchless decision (extends v96)
 * ------------------------------------------------------------------
 *
 *     cos_v97_compose_decision(v96_composed_ok, v97_ok)
 *         = v96_composed_ok & v97_ok
 */

#ifndef COS_V97_RL_H
#define COS_V97_RL_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V97_SENTINEL      0x97EA12D0u
#define COS_V97_NSTATES       8u              /* tabular MDP |S| */
#define COS_V97_NACTIONS      4u              /* |A|              */
#define COS_V97_HORIZON       64u             /* trajectory length */
#define COS_V97_Q1            65536           /* 1.0 Q16.16      */
#define COS_V97_Q_CLAMP       (64 * 65536)    /* |Q| ≤ 64.0       */
#define COS_V97_GAMMA         58982           /* γ  = 0.9   Q16.16 */
#define COS_V97_LAMBDA        62259           /* λ  = 0.95  Q16.16 */
#define COS_V97_ALPHA_RSHIFT  4u              /* α  = 1/16         */
#define COS_V97_CLIP_EPS      6553            /* ε  = 0.10  Q16.16 */

typedef int32_t cos_v97_q16_t;

typedef struct {
    cos_v97_q16_t Q [COS_V97_NSTATES][COS_V97_NACTIONS];
    uint32_t      updates;
    uint32_t      clamp_violations;
    uint32_t      sentinel;
} cos_v97_agent_t;

/* ---- Q-learning ---------------------------------------------------- */

void cos_v97_agent_init(cos_v97_agent_t *a, uint64_t seed);

/* Bellman update, branchless in its numeric path. */
void cos_v97_q_update(cos_v97_agent_t *a,
                      uint32_t s, uint32_t act,
                      cos_v97_q16_t reward,
                      uint32_t s_next,
                      uint32_t done);

/* argmax_a Q[s,a] — deterministic tie-break on the lowest index. */
uint32_t cos_v97_greedy(const cos_v97_agent_t *a, uint32_t s);

/* V(s) = max_a Q[s,a] */
cos_v97_q16_t cos_v97_value(const cos_v97_agent_t *a, uint32_t s);

/* ---- GAE ---------------------------------------------------------- */

/* Computes A_t for t = 0..T-1 (reverse scan).  `values` must have
 * length T+1 (the final bootstrap value is `values[T]`). */
void cos_v97_gae(const cos_v97_q16_t *rewards,
                 const cos_v97_q16_t *values,
                 uint32_t T,
                 cos_v97_q16_t *advantages_out);

/* ---- PPO-clip envelope ------------------------------------------- */

/* Returns L_clip(ratio, A) in Q16.16.
 *   ratio  ∈ [0, 2·Q1]       (policy probability ratio)
 *   A      ∈ Q16.16           (advantage estimate)
 *
 * Branchless: uses saturating clamp, no conditional branches on the
 * data path. */
cos_v97_q16_t cos_v97_ppo_clip(cos_v97_q16_t ratio, cos_v97_q16_t A);

/* ---- Verdict ------------------------------------------------------ */

uint32_t cos_v97_ok(const cos_v97_agent_t *a);
uint32_t cos_v97_compose_decision(uint32_t v96_composed_ok,
                                  uint32_t v97_ok);

#ifdef __cplusplus
}
#endif

#endif /* COS_V97_RL_H */
