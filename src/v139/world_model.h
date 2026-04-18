/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (c) 2026 Spektre Labs / Lauri Rainio
 *
 * v139 σ-WorldModel — linear latent predictor.
 *
 * LLMs predict the next *token*.  A world model predicts the
 * next *state*.  v139 builds the smallest useful version of
 * that: given a sequence of D-dim latent states (s_0, s_1, …,
 * s_T), fit a linear transition A ∈ ℝ^{D×D} by least squares,
 * then predict s_{t+1} = A · s_t and measure σ_world, the
 * normalised prediction residual.
 *
 * Three primitives (all pure C, deterministic, no GPU):
 *
 *   1. Least-squares fit.  A = S_next · pinv(S_curr) implemented
 *      via the normal equations (A S_curr S_curr^T = S_next S_curr^T)
 *      solved with Gauss-Jordan with partial pivoting.  Works on
 *      well-conditioned data; for rank-deficient problems the
 *      caller supplies more pairs than D.
 *
 *   2. Prediction + σ_world.  One D×D mat-mul gives the predicted
 *      next state.  σ_world = ‖s_actual − s_pred‖ / ‖s_actual‖ ∈
 *      [0, ∞);  small σ_world means "the world is familiar".
 *
 *   3. Multi-step rollout.  Applies A iteratively for N horizon
 *      steps from a seed state; the caller inspects the σ_world
 *      trajectory.  A monotonically-rising σ_world is v139's
 *      "the plan is breaking down" signal and feeds v121 planning.
 *
 * v139.0 ships the full pipeline at D = 16 so the merge-gate
 * solves the normal equations in microseconds and lets CI verify
 * the whole round-trip deterministically.  v139.1 extends to
 * D = 2560 driven by v101's real BitNet layer-15 hidden states,
 * with rank-r truncated SVD and on-disk persistence at
 * `models/v139/world_model_lr64.bin`.
 */
#ifndef COS_V139_WORLD_MODEL_H
#define COS_V139_WORLD_MODEL_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V139_MAX_DIM     64
#define COS_V139_MAX_ROLLOUT 32

typedef struct cos_v139_model {
    int   dim;                                    /* D ≤ MAX_DIM     */
    float A[COS_V139_MAX_DIM * COS_V139_MAX_DIM]; /* row-major D×D   */
    float mean_l2_residual;                       /* training-set σ  */
    int   n_training_pairs;
} cos_v139_model_t;

typedef struct cos_v139_rollout {
    int     horizon;
    float   states [COS_V139_MAX_ROLLOUT * COS_V139_MAX_DIM];
    float   sigma_world[COS_V139_MAX_ROLLOUT];
    int     monotone_rising;   /* σ_world[i] ≥ σ_world[i-1] ∀ i */
} cos_v139_rollout_t;

/* Fit A by normal equations.  `pairs_curr` and `pairs_next` are
 * row-major [n, dim].  Returns 0 on success, <0 on singular
 * normal matrix.  Fills m->A, m->mean_l2_residual, m->n_training_pairs. */
int   cos_v139_fit(cos_v139_model_t *m,
                   int dim, int n,
                   const float *pairs_curr,
                   const float *pairs_next);

/* One-step prediction.  out_next[i] = Σ_j A[i,j] * s[j]. */
void  cos_v139_predict(const cos_v139_model_t *m,
                       const float *s, float *out_next);

/* σ_world on a single (actual, pred) pair.
 * If ‖actual‖ < eps, returns ‖actual − pred‖ directly. */
float cos_v139_sigma_world(const float *s_actual,
                           const float *s_pred, int dim);

/* Multi-step rollout starting from seed.  Each step also needs the
 * *actual* next state to compute σ_world; the caller supplies the
 * full actual trajectory of length horizon+1.  `actual` may be
 * NULL to skip σ_world accounting (pure prediction). */
int   cos_v139_rollout(const cos_v139_model_t *m,
                       const float *seed, int horizon,
                       const float *actual /* nullable */,
                       cos_v139_rollout_t *out);

/* Deterministic synthetic state sequence generator: a stable
 * contraction ρ in random orthogonal basis, returned as row-major
 * [T+1, dim].  Used for the self-test and by callers that want a
 * reproducible toy environment. */
void  cos_v139_synthetic_trajectory(float *out, int dim, int t_plus_one,
                                    float contraction /* 0.90 default */,
                                    uint64_t seed);

/* JSON summary of the model (shape + mean residual). */
int   cos_v139_to_json(const cos_v139_model_t *m, char *out, size_t cap);

int   cos_v139_self_test(void);

#ifdef __cplusplus
}
#endif
#endif
