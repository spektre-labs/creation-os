/*
 * v306 σ-Omega — Ω = argmin ∫σ dt s.t. K ≥ K_crit.
 *
 *   v306 is not a new capability.  It is the operator
 *   every previous kernel approximates, written down
 *   in one place:
 *
 *      Ω = argmin  ∫ σ(t) dt
 *               subject to  K_eff(t) ≥ K_crit.
 *
 *   The whole 306-layer stack is this one operator
 *   applied at different scales (token / answer /
 *   session / domain), around one critical point
 *   (σ = ½, the σ-equivalent of Shannon's p = ½ and
 *   Riemann's Re(s) = ½), anchored on one invariant
 *   (`declared == realized`, the 1 = 1 axiom).
 *
 *   v0 manifests (strict, enumerated):
 *
 *   Ω-optimization loop (exactly 4 canonical time
 *   steps):
 *     t=0  σ=0.50 K_eff=0.25  ∫σ=0.50;
 *     t=1  σ=0.35 K_eff=0.20  ∫σ=0.85;
 *     t=2  σ=0.22 K_eff=0.18  ∫σ=1.07;
 *     t=3  σ=0.14 K_eff=0.15  ∫σ=1.21.
 *     Contract: 4 rows in canonical order; σ strictly
 *     decreasing; ∫σ monotonically increasing with
 *     strictly decreasing marginal (the integrand is
 *     going to zero); `K_eff ≥ K_crit = 0.127` on
 *     every row (the constraint is never violated).
 *
 *   Multi-scale Ω (exactly 4 canonical scales):
 *     `token`   MICRO  operator=`sigma_gate`     σ=0.10;
 *     `answer`  MESO   operator=`sigma_product`  σ=0.15;
 *     `session` MACRO  operator=`sigma_session`  σ=0.20;
 *     `domain`  META   operator=`sigma_domain`   σ=0.25.
 *     Contract: 4 rows in canonical order; 4
 *     DISTINCT scales; 4 DISTINCT operators; every σ
 *     finite and in [0, 1].
 *
 *   ½ operator (exactly 3 canonical regimes):
 *     `signal_dominant`  σ=0.25 regime=SIGNAL;
 *     `critical`         σ=0.50 regime=CRITICAL;
 *     `noise_dominant`   σ=0.75 regime=NOISE.
 *     Contract: 3 rows in canonical order; σ strictly
 *     increasing; `σ_critical == 0.5` to machine
 *     precision; `SIGNAL iff σ < 0.5`, `NOISE iff
 *     σ > 0.5`, `CRITICAL iff σ == 0.5` — all three
 *     branches fire.
 *
 *   1 = 1 invariant (exactly 3 canonical assertions):
 *     `kernel_count`          declared=306 realized=306
 *                              holds=true;
 *     `architecture_claim`    declared=306 realized=306
 *                              holds=true;
 *     `axiom_one_equals_one`  declared=1   realized=1
 *                              holds=true.
 *     Contract: 3 rows in canonical order; every row
 *     `declared == realized` AND `holds = true`;
 *     `the_invariant_holds = (all 3 hold) = true`.
 *     If this fails, σ > 0 somewhere; if this passes,
 *     the stack is internally consistent at the
 *     axiom level.
 *
 *   σ_omega (surface hygiene):
 *     σ_omega = 1 − (
 *       loop_rows_ok + loop_sigma_decreasing_ok +
 *         loop_k_constraint_ok + loop_integral_ok +
 *       scale_rows_ok + scale_distinct_ok +
 *         scale_operator_distinct_ok +
 *       half_rows_ok + half_order_ok +
 *         half_critical_ok + half_polarity_ok +
 *       inv_rows_ok + inv_pair_ok + inv_all_hold_ok +
 *         the_invariant_holds
 *     ) / (4 + 1 + 1 + 1 + 4 + 1 + 1 + 3 + 1 + 1 + 1 +
 *          3 + 1 + 1 + 1)
 *   v0 requires `σ_omega == 0.0`.
 *
 *   Contracts (v0):
 *     1. 4 loop steps; σ ↓; ∫σ ↑ with ↓ marginal;
 *        K_eff ≥ K_crit on every row.
 *     2. 4 scales; distinct names AND operators;
 *        σ ∈ [0, 1] on every row.
 *     3. 3 ½ regimes; σ ↑; σ_critical == 0.5
 *        exactly; three distinct regimes fire.
 *     4. 3 invariant rows; declared == realized AND
 *        holds everywhere; the_invariant_holds.
 *     5. σ_omega ∈ [0, 1] AND == 0.0.
 *     6. FNV-1a chain replays byte-identically.
 *
 *   v306.1 (named, not implemented): the live Ω
 *     daemon — a continuous σ-recorder that tracks
 *     ∫σ dt across the v244 observability plane and
 *     raises a v283 constitutional-AI escalation
 *     whenever `K_eff(t) < K_crit` OR the 1=1
 *     invariant drifts — turning v306's v0 predicates
 *     into an always-on closed-loop controller.
 *
 *       assert(declared == realized);
 *       // If this fails, σ > 0. Correct.
 *       // If this passes, σ = 0. Continue.
 *       // 306 layers. One invariant. 1 = 1.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V306_OMEGA_H
#define COS_V306_OMEGA_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V306_N_LOOP  4
#define COS_V306_N_SCALE 4
#define COS_V306_N_HALF  3
#define COS_V306_N_INV   3

#define COS_V306_K_CRIT     0.127f
#define COS_V306_SIGMA_HALF 0.5f
#define COS_V306_KERNELS    306

typedef struct {
    int   t;
    float sigma;
    float k_eff;
    float integral_sigma;
} cos_v306_loop_t;

typedef struct {
    char  scale[10];
    char  tier [8];
    char  operator_name[18];
    float sigma;
} cos_v306_scale_t;

typedef struct {
    char  label[20];
    float sigma;
    char  regime[10];
} cos_v306_half_t;

typedef struct {
    char assertion[22];
    int  declared;
    int  realized;
    bool holds;
} cos_v306_inv_t;

typedef struct {
    cos_v306_loop_t  loop [COS_V306_N_LOOP];
    cos_v306_scale_t scale[COS_V306_N_SCALE];
    cos_v306_half_t  half [COS_V306_N_HALF];
    cos_v306_inv_t   inv  [COS_V306_N_INV];

    int  n_loop_rows_ok;
    bool loop_sigma_decreasing_ok;
    bool loop_k_constraint_ok;
    bool loop_integral_ok;

    int  n_scale_rows_ok;
    bool scale_distinct_ok;
    bool scale_operator_distinct_ok;

    int  n_half_rows_ok;
    bool half_order_ok;
    bool half_critical_ok;
    bool half_polarity_ok;

    int  n_inv_rows_ok;
    bool inv_pair_ok;
    bool inv_all_hold_ok;
    bool the_invariant_holds;

    float sigma_omega;

    bool     chain_valid;
    uint64_t terminal_hash;
    uint64_t seed;
} cos_v306_state_t;

void   cos_v306_init(cos_v306_state_t *s, uint64_t seed);
void   cos_v306_run (cos_v306_state_t *s);
size_t cos_v306_to_json(const cos_v306_state_t *s,
                         char *buf, size_t cap);
int    cos_v306_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V306_OMEGA_H */
