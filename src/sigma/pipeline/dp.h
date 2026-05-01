/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/*
 * σ-Differential-Privacy — (ε, δ)-DP noise layer over σ-federation.
 *
 * σ-federation already σ-gates poisoned / noisy contributors and
 * weights the remainder by trust; what it does NOT provide is a
 * MATHEMATICAL privacy guarantee that a single honest contributor's
 * training data cannot be reconstructed from the aggregated
 * Δweight.  σ_dp adds that layer: before federation averages the
 * contributions, every Δw vector is clipped to an L2 norm and
 * perturbed with calibrated Laplace noise; an ε-budget is tracked
 * across rounds so the caller can refuse further updates once the
 * cumulative privacy loss exceeds a declared total.
 *
 * Mechanism: classical Laplace mechanism for (ε, 0)-DP.  Given
 *   1.  clip_norm  C  — the sensitivity bound,
 *   2.  epsilon    ε  — the per-round privacy budget,
 * the output satisfies ε-DP if every entry of the perturbed vector
 * is g'[j] = clip(g, C)[j] + Lap(C / ε), and successive rounds
 * compose additively:  ε_total = Σ ε_round.
 *
 * σ coupling:  σ_dp measures how much the Laplace noise distorted
 * the useful signal.  Given the clipped gradient g_c and the
 * perturbed gradient g_p,
 *     σ_dp = clip01(‖g_p − g_c‖₂ / (‖g_c‖₂ + ε_machine))
 * i.e. the fraction of the signal that was replaced by noise.
 * Downstream σ-federation uses σ_dp to weight contributions: a
 * contributor whose σ_dp is close to 1 carried ~all noise and is
 * aggregated with w ≈ 0.
 *
 * Contracts (v0):
 *   1. init enforces ε > 0, δ ∈ [0, 1), clip_norm > 0,
 *      ε_total > 0, ε_spent ≥ 0, ε_spent ≤ ε_total.
 *   2. add_noise is deterministic given (gradient, seed); the RNG
 *      is a host-provided Philox-style counter-based LCG so a
 *      replayed seed + gradient yields a bit-identical output.
 *   3. add_noise refuses the round (returns BUDGET_EXHAUSTED) when
 *      ε_spent + ε > ε_total; the gradient is untouched in that
 *      case — caller is responsible for aborting the federation.
 *   4. σ_dp ∈ [0, 1], NaN-free; 0 when ‖g_c‖ = 0 (zero-gradient
 *      contributor contributes nothing either way).
 *
 * GDPR stance: ε_total is the "right to be forgotten" knob — set
 * it to 0 and no further update is admitted for that node, which
 * means the honest node's historical data cannot be used to update
 * the global model further.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef COS_SIGMA_DP_H
#define COS_SIGMA_DP_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum cos_dp_status {
    COS_DP_OK                  =  0,
    COS_DP_ERR_ARG             = -1,
    COS_DP_ERR_NAN             = -2,
    COS_DP_ERR_BUDGET_EXHAUSTED= -3,
};

typedef struct {
    float epsilon;          /* per-round privacy budget (> 0)         */
    float delta;            /* failure probability      [0, 1)        */
    float clip_norm;        /* L2 clipping bound (= sensitivity)      */
    float epsilon_spent;    /* cumulative privacy loss                */
    float epsilon_total;    /* cap; epsilon_spent may not exceed this */
    uint64_t rng_state;     /* counter-based LCG; caller seeds        */
    int    rounds;          /* how many add_noise calls have landed   */
} cos_sigma_dp_state_t;

typedef struct {
    int    status;                /* COS_DP_OK or negative            */
    float  clip_scale;            /* C / ‖g‖ if clipped, else 1.0     */
    float  noise_scale;           /* Laplace scale C / ε              */
    float  pre_clip_norm;         /* ‖g‖   before clipping            */
    float  post_clip_norm;        /* ‖g_c‖ after clipping (≤ C)       */
    float  post_noise_norm;       /* ‖g_p‖ after adding noise         */
    float  noise_l2;              /* ‖g_p − g_c‖₂                      */
    float  sigma_dp;              /* noise_l2 / (post_clip_norm + ε)  */
    float  epsilon_remaining;     /* ε_total − ε_spent (post-round)   */
    int    budget_exhausted;      /* 1 ⇔ epsilon_remaining < epsilon  */
} cos_sigma_dp_report_t;

/* -------- State lifecycle ------------------------------------------ */

int cos_sigma_dp_init(cos_sigma_dp_state_t *st,
                      float epsilon,
                      float delta,
                      float clip_norm,
                      float epsilon_total,
                      uint64_t seed);

/* -------- Per-round operations ------------------------------------- */

/* Clip `grad[0..n_params-1]` in place to L2 norm ≤ state.clip_norm,
 * add Laplace noise, update ε_spent, and emit a σ_dp receipt.  If
 * ε_spent + ε > ε_total the gradient is left untouched and the
 * report carries status = COS_DP_ERR_BUDGET_EXHAUSTED. */
int cos_sigma_dp_add_noise(cos_sigma_dp_state_t *st,
                           float *grad,
                           int n_params,
                           cos_sigma_dp_report_t *report);

/* Compute σ_dp from a caller-supplied (pre, post) pair without
 * touching ε-budget.  Handy for retro-analysis of a round that was
 * already applied. */
float cos_sigma_dp_compute_sigma(const float *g_clipped,
                                 const float *g_perturbed,
                                 int n_params);

/* -------- Budget introspection ------------------------------------- */

static inline float cos_sigma_dp_spent(const cos_sigma_dp_state_t *st) {
    return st ? st->epsilon_spent : 0.0f;
}
static inline float cos_sigma_dp_remaining(const cos_sigma_dp_state_t *st) {
    return st ? (st->epsilon_total - st->epsilon_spent) : 0.0f;
}
static inline int   cos_sigma_dp_rounds_left(const cos_sigma_dp_state_t *st) {
    if (!st || st->epsilon <= 0.0f) return 0;
    float r = cos_sigma_dp_remaining(st);
    if (r <= 0.0f) return 0;
    return (int)(r / st->epsilon);
}

/* -------- Self-test ------------------------------------------------ */

int cos_sigma_dp_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_SIGMA_DP_H */
