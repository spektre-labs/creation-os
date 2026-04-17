/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Creation OS v86 — σ-JEPA (latent-space predictive world model).
 *
 * ------------------------------------------------------------------
 *  What v86 is
 * ------------------------------------------------------------------
 *
 * v86 is the non-generative latent world-model plane in the style of
 * Yann LeCun's Joint Embedding Predictive Architecture (JEPA, 2022)
 * and Meta's V-JEPA 2 (June 2025).  Instead of reconstructing pixels,
 * the agent learns to predict *future latent embeddings* z_t+1 from
 * the current embedding z_t and an action a_t.  All arithmetic is
 * Q16.16 integer; collapse is prevented by a VICReg-style invariance
 * / variance / covariance triple (Bardes, Ponce, LeCun 2022) adapted
 * to integer state.  No malloc on the hot path; branchless-ish.
 *
 * Eight primitives:
 *
 *   1. cos_v86_encode          — online encoder E_θ(x) → z ∈ Q16.16^D
 *   2. cos_v86_target_encode   — EMA target encoder E_ξ(x) → z'
 *   3. cos_v86_predict         — predictor P_φ(z, a) → ẑ
 *   4. cos_v86_pred_loss       — L1(ẑ, z') in Q16.16
 *   5. cos_v86_vicreg_invar    — invariance term ||E(x) - E'(x)||_1
 *   6. cos_v86_vicreg_var      — variance term (hinge below γ)
 *   7. cos_v86_vicreg_covar    — off-diagonal covariance penalty
 *   8. cos_v86_ema_update      — EMA step for the target encoder
 *
 * The encoder and predictor are small Q16.16 integer MLPs whose
 * weights live inside the state struct.  No FP.  No external BLAS.
 * The goal is *world-model determinism* — every receipt is exact,
 * every rollout is reproducible bit-for-bit.
 *
 * ------------------------------------------------------------------
 *  Composed 26-bit branchless decision (extends v85)
 * ------------------------------------------------------------------
 *
 *     cos_v86_compose_decision(v85_composed_ok, v86_ok)
 *         = v85_composed_ok & v86_ok
 *
 * `v86_ok = 1` iff the sentinel is intact, the rollout error is
 * within a declared budget, and VICReg variance stays above the
 * anti-collapse floor over the measurement window.
 */

#ifndef COS_V86_JEPA_H
#define COS_V86_JEPA_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V86_SENTINEL     0x9E6AC0DEu
#define COS_V86_DIM          16u    /* latent dim (compile-time) */
#define COS_V86_OBS          16u    /* observation dim */
#define COS_V86_ACT          4u     /* action dim */

typedef int32_t cos_v86_q16_t;

typedef struct {
    /* Online encoder: z = W_e · x + b_e, relu, Q16.16. */
    cos_v86_q16_t W_e[COS_V86_DIM][COS_V86_OBS];
    cos_v86_q16_t b_e[COS_V86_DIM];

    /* Target encoder (EMA of W_e, b_e). */
    cos_v86_q16_t W_t[COS_V86_DIM][COS_V86_OBS];
    cos_v86_q16_t b_t[COS_V86_DIM];

    /* Predictor: ẑ = W_p · [z ; a] + b_p. */
    cos_v86_q16_t W_p[COS_V86_DIM][COS_V86_DIM + COS_V86_ACT];
    cos_v86_q16_t b_p[COS_V86_DIM];

    /* Stats. */
    cos_v86_q16_t last_pred_err;         /* L1(ẑ, z') */
    cos_v86_q16_t last_var;              /* min over dims */
    cos_v86_q16_t last_covar;            /* sum |C_ij| for i≠j */
    uint32_t      rollout_budget;        /* in Q16.16 */
    uint32_t      variance_floor;        /* in Q16.16 */
    uint32_t      invariant_violations;
    uint32_t      steps;
    uint32_t      sentinel;
} cos_v86_jepa_t;

/* Init: loads a reproducible reference MLP keyed by `seed`.  The
 * weights are drawn from a deterministic integer LCG so every JEPA
 * started with the same seed is bit-equal. */
void cos_v86_jepa_init(cos_v86_jepa_t *j, uint64_t seed);

/* Encoders. */
void cos_v86_encode       (const cos_v86_jepa_t *j,
                           const cos_v86_q16_t x[COS_V86_OBS],
                           cos_v86_q16_t       z[COS_V86_DIM]);
void cos_v86_target_encode(const cos_v86_jepa_t *j,
                           const cos_v86_q16_t x[COS_V86_OBS],
                           cos_v86_q16_t       z[COS_V86_DIM]);

/* Predictor. */
void cos_v86_predict(const cos_v86_jepa_t *j,
                     const cos_v86_q16_t z[COS_V86_DIM],
                     const cos_v86_q16_t a[COS_V86_ACT],
                     cos_v86_q16_t       zhat[COS_V86_DIM]);

/* Loss terms (all L1 / hinge / abs, integer). */
cos_v86_q16_t cos_v86_pred_loss   (const cos_v86_q16_t zhat[COS_V86_DIM],
                                   const cos_v86_q16_t ztgt[COS_V86_DIM]);

cos_v86_q16_t cos_v86_vicreg_invar(const cos_v86_q16_t z [COS_V86_DIM],
                                   const cos_v86_q16_t zp[COS_V86_DIM]);

/* Batch-variance: given a batch of N latents (concatenated row-major),
 * returns the minimum per-dim variance in Q16.16.  A collapsed
 * representation has small variance. */
cos_v86_q16_t cos_v86_vicreg_var(const cos_v86_q16_t *Z,
                                 uint32_t N);

cos_v86_q16_t cos_v86_vicreg_covar(const cos_v86_q16_t *Z,
                                   uint32_t N);

/* EMA step: W_t ← (1-τ) W_t + τ W_e with τ = 1/8 (numerator/denom
 * chosen so τ is exact in Q16.16 integer). */
void cos_v86_ema_update(cos_v86_jepa_t *j);

/* One-step world-model roll: encode, predict, target-encode, measure
 * prediction error, extend step counter.  Returns the L1 error. */
cos_v86_q16_t cos_v86_rollout_step(cos_v86_jepa_t *j,
                                   const cos_v86_q16_t x_t  [COS_V86_OBS],
                                   const cos_v86_q16_t a_t  [COS_V86_ACT],
                                   const cos_v86_q16_t x_tp1[COS_V86_OBS]);

/* Gate + compose. */
uint32_t cos_v86_ok(const cos_v86_jepa_t *j);
uint32_t cos_v86_compose_decision(uint32_t v85_composed_ok,
                                  uint32_t v86_ok);

#ifdef __cplusplus
}
#endif

#endif /* COS_V86_JEPA_H */
