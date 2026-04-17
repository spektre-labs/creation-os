/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Creation OS v90 — σ-Hierarchical (hierarchical active inference).
 *
 * ------------------------------------------------------------------
 *  What v90 is
 * ------------------------------------------------------------------
 *
 * v90 extends the flat PLAN→ROLL→SURPRISE→ENERGY learner loop of
 * v83 into a *hierarchical* predictive-coding stack in the style of
 * Friston's Renormalising Generative Models (RGM, 2025) and
 * Van de Maele et al.'s Schema-based Hierarchical Active Inference
 * (S-HAI, 2026).  The architecture is a three-level tower:
 *
 *      L2  (schema / slow prior)
 *         ↕  top-down μ_hat_2 → L1  ;  bottom-up ε_1 → L2
 *      L1  (mid-level dynamics)
 *         ↕
 *      L0  (observation)
 *
 * Each level runs its own one-step prediction cycle with its own
 * precision weight π_l ∈ Q16.16.  A level's *prediction error* is
 *
 *     ε_l = o_l - μ_hat_l             (o_L = prior above)
 *
 * and the free-energy-equivalent at level l is
 *
 *     F_l = π_l * ε_l^2
 *
 * Six primitives (Q16.16 integer, branchless):
 *
 *   1. cos_v90_hi_init           — three-level stack with precisions
 *   2. cos_v90_step              — observe o_0, cascade predictions
 *                                  top-down, compute per-level
 *                                  prediction errors bottom-up,
 *                                  accumulate free energy.
 *   3. cos_v90_total_free_energy — sum F_0 + F_1 + F_2
 *   4. cos_v90_update_schema     — consolidate the slow prior with
 *                                  the accumulated L2 error (EMA).
 *   5. cos_v90_receipt           — SHAKE-256 (via v81) over the
 *                                  current tower state.
 *   6. cos_v90_reset_schema      — hard reset of the L2 schema.
 *
 * ------------------------------------------------------------------
 *  Composed 30-bit branchless decision (extends v89)
 * ------------------------------------------------------------------
 *
 *     cos_v90_compose_decision(v89_composed_ok, v90_ok)
 *         = v89_composed_ok & v90_ok
 *
 * `v90_ok = 1` iff sentinel is intact, the accumulated total free
 * energy stays within the declared ceiling, and the receipt chain
 * has not been externally tampered with (chain hash consistency).
 */

#ifndef COS_V90_HIERARCHICAL_H
#define COS_V90_HIERARCHICAL_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V90_SENTINEL   0x1ECBCAFEu
#define COS_V90_LEVELS     3u
#define COS_V90_DIM        8u

typedef int32_t cos_v90_q16_t;

typedef struct {
    cos_v90_q16_t mu   [COS_V90_LEVELS][COS_V90_DIM];   /* posterior mean */
    cos_v90_q16_t mu_pr[COS_V90_LEVELS][COS_V90_DIM];   /* top-down prior */
    cos_v90_q16_t err  [COS_V90_LEVELS][COS_V90_DIM];
    cos_v90_q16_t pi   [COS_V90_LEVELS];                /* precision π_l  */

    cos_v90_q16_t total_free_energy;                    /* F_sum, Q16.16 */
    cos_v90_q16_t last_free_energy;                     /* F at the last step */
    uint32_t      free_energy_ceiling;                  /* per-step Q16.16 ceiling */
    uint32_t      steps;

    uint8_t       chain_hash[32];                       /* SHAKE-256 rolling */
    uint32_t      invariant_violations;
    uint32_t      sentinel;
} cos_v90_hi_t;

/* Init. */
void cos_v90_hi_init(cos_v90_hi_t *h, uint64_t seed);

/* One hierarchical step.  Bottom-up observation o ∈ R^D, slow prior
 * p ∈ R^D at the top (can be zero).  Returns the newly accumulated
 * per-step free energy. */
cos_v90_q16_t cos_v90_step(cos_v90_hi_t *h,
                           const cos_v90_q16_t o[COS_V90_DIM],
                           const cos_v90_q16_t p[COS_V90_DIM]);

/* Total free energy. */
cos_v90_q16_t cos_v90_total_free_energy(const cos_v90_hi_t *h);

/* Schema consolidation. */
void cos_v90_update_schema(cos_v90_hi_t *h);

/* Rolling SHAKE-256 receipt (32 bytes). */
void cos_v90_receipt(const cos_v90_hi_t *h, uint8_t out[32]);

/* Reset the top-level schema. */
void cos_v90_reset_schema(cos_v90_hi_t *h);

/* Gate + compose. */
uint32_t cos_v90_ok(const cos_v90_hi_t *h);
uint32_t cos_v90_compose_decision(uint32_t v89_composed_ok,
                                  uint32_t v90_ok);

#ifdef __cplusplus
}
#endif

#endif /* COS_V90_HIERARCHICAL_H */
