/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Creation OS v96 — σ-Diffusion (integer rectified-flow / DDIM sampler).
 *
 * ------------------------------------------------------------------
 *  What v96 is
 * ------------------------------------------------------------------
 *
 * A deterministic, integer-only rectified-flow / DDIM-style sampler
 * over a fixed linear noise schedule.  No floating point, no RNG in
 * the hot path, no malloc.  Q16.16 throughout.
 *
 * Schedule:
 *     α_bar[0]   = Q1                      (= 1.0)
 *     α_bar[T]   = 0                       (= pure "noise" marker)
 *     α_bar[t]   = Q1 − t * Q1 / T         (linear, strictly decreasing)
 *
 * Forward (corruption):
 *     x_t = α_bar[t] ⊙ x_0 + (Q1 − α_bar[t]) ⊙ x_1          (rectified flow)
 *
 * Reverse (deterministic DDIM / Euler along the straight line):
 *     v     = x_1 − x_0                                       (flow velocity)
 *     x_{t} = x_{t+1} − v · (Q1 / T)                          (straight-line integrator)
 *
 * Invariants checked at runtime:
 *   • α_bar schedule strictly decreasing
 *   • forward ∘ reverse ≡ identity at every step pair (to within Q16.16 rounding)
 *   • energy ‖x_t − x_0‖₁ monotone non-increasing during denoise
 *   • sentinel intact
 *
 * ------------------------------------------------------------------
 *  Composed 36-bit branchless decision (extends v95)
 * ------------------------------------------------------------------
 *
 *     cos_v96_compose_decision(v95_composed_ok, v96_ok)
 *         = v95_composed_ok & v96_ok
 */

#ifndef COS_V96_DIFFUSION_H
#define COS_V96_DIFFUSION_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V96_SENTINEL   0x96DDF10Eu
#define COS_V96_T          16u                 /* diffusion steps */
#define COS_V96_D          8u                  /* data dim */
#define COS_V96_Q1         65536               /* 1.0 in Q16.16 */

typedef int32_t cos_v96_q16_t;

typedef struct {
    cos_v96_q16_t alpha_bar[COS_V96_T + 1u];    /* decreasing Q1 → 0 */
    cos_v96_q16_t x0 [COS_V96_D];               /* clean data anchor */
    cos_v96_q16_t x1 [COS_V96_D];               /* "noise" endpoint   */
    cos_v96_q16_t x  [COS_V96_D];               /* current sample     */
    int64_t       last_distance_to_x0;          /* ‖x − x0‖₁ (Q16.16 sum) */
    uint32_t      step;                         /* current diffusion step (0..T) */
    uint32_t      monotone_violations;
    uint32_t      identity_violations;
    uint32_t      sentinel;
} cos_v96_diffuser_t;

/* Initialises the schedule, sets x0 = 0, x1 = 0, step = T (pure noise). */
void cos_v96_diffuser_init(cos_v96_diffuser_t *d);

/* Loads the clean anchor and the noise endpoint. */
void cos_v96_set_endpoints(cos_v96_diffuser_t *d,
                           const cos_v96_q16_t x0[COS_V96_D],
                           const cos_v96_q16_t x1[COS_V96_D]);

/* Forward corruption to exact step t ∈ [0, T].
 *     x = α_bar[t]·x0 + (Q1 − α_bar[t])·x1                               */
void cos_v96_forward_to(cos_v96_diffuser_t *d, uint32_t t);

/* One deterministic reverse step: x ← x − v·(Q1/T) where v = x1 − x0.
 * Decrements step; clamps at 0.                                          */
void cos_v96_reverse_step(cos_v96_diffuser_t *d);

/* Runs `steps` reverse steps. */
void cos_v96_reverse_run(cos_v96_diffuser_t *d, uint32_t steps);

/* L1 distance from current sample to x0. */
int64_t cos_v96_distance_to_x0(const cos_v96_diffuser_t *d);

/* α_bar strictly decreasing across the full schedule.           */
uint32_t cos_v96_schedule_ok(const cos_v96_diffuser_t *d);

uint32_t cos_v96_ok(const cos_v96_diffuser_t *d);
uint32_t cos_v96_compose_decision(uint32_t v95_composed_ok, uint32_t v96_ok);

#ifdef __cplusplus
}
#endif

#endif /* COS_V96_DIFFUSION_H */
