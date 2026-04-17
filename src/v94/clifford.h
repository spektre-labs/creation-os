/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Creation OS v94 — σ-Clifford (geometric algebra Cl(3,0) layers,
 * CliffordNet-style equivariant feature mixing, 2026).
 *
 * ------------------------------------------------------------------
 *  What v94 is
 * ------------------------------------------------------------------
 *
 * Cl(3,0) has the 8-dimensional multivector basis
 *
 *     { 1, e1, e2, e3, e12, e13, e23, e123 }
 *
 * with e_i·e_i = +1 and e_i·e_j = -e_j·e_i for i ≠ j.
 *
 * v94 gives an integer (Q16.16), deterministic, branchless
 * implementation of:
 *
 *   1. geometric product       uv  = u·v + u∧v   (full 8×8 MV)
 *   2. wedge (exterior) product u∧v
 *   3. inner (dot)   product   u·v
 *   4. reverse                 ~u  (grade-k sign: −1 iff k(k−1)/2 odd)
 *   5. grade-k projector       <u>_k
 *   6. a single equivariant layer  y = σ(uv) + bias     where σ is
 *      a saturating clamp, and (u, v) are two input multivectors.
 *
 * The Clifford group action (orthogonal transforms) is preserved by
 * the geometric product; using MVs as features and GP as the mixer
 * is the "all you need" ansatz of CliffordNet.  We verify in the
 * self-test that the product is associative, that uv = u·v + u∧v,
 * and that rotors r x r̃ implement 3-D rotations to Q16.16 precision.
 *
 * ------------------------------------------------------------------
 *  Composed 34-bit branchless decision (extends v93)
 * ------------------------------------------------------------------
 *
 *     cos_v94_compose_decision(v93_composed_ok, v94_ok)
 *         = v93_composed_ok & v94_ok
 *
 * `v94_ok = 1` iff the multiplication table is well-formed
 * (signs ∈ {−1,+1}, indices ∈ [0..7]) and sentinel is intact.
 */

#ifndef COS_V94_CLIFFORD_H
#define COS_V94_CLIFFORD_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V94_SENTINEL    0x94C11FF0u
#define COS_V94_DIM         8u      /* 2^3 multivector dim for Cl(3,0) */

typedef int32_t cos_v94_q16_t;
typedef cos_v94_q16_t cos_v94_mv_t[COS_V94_DIM];

typedef struct {
    /* Pre-computed multiplication table: product[A][B] = result_index,
     * sign[A][B] = ±1.  Both 3-bit A and 3-bit B, result index is
     * A XOR B (e_i·e_i = +1 in Cl(3,0) so no auxiliary basis). */
    uint8_t  prod_idx  [COS_V94_DIM][COS_V94_DIM];
    int8_t   prod_sign [COS_V94_DIM][COS_V94_DIM];
    uint32_t sentinel;
} cos_v94_algebra_t;

void cos_v94_alg_init(cos_v94_algebra_t *a);

/* Geometric product: out = u * v. */
void cos_v94_gp   (const cos_v94_algebra_t *a,
                   const cos_v94_mv_t u, const cos_v94_mv_t v,
                   cos_v94_mv_t out);

/* Reverse: conjugation of the geometric product. */
void cos_v94_rev  (const cos_v94_mv_t u, cos_v94_mv_t out);

/* Grade-k projection. */
void cos_v94_grade(const cos_v94_mv_t u, uint32_t k, cos_v94_mv_t out);

/* Inner product (symmetric part): u·v = (uv + vu) / 2. */
void cos_v94_inner(const cos_v94_algebra_t *a,
                   const cos_v94_mv_t u, const cos_v94_mv_t v,
                   cos_v94_mv_t out);

/* Wedge product (antisymmetric part): u∧v = (uv − vu) / 2. */
void cos_v94_wedge(const cos_v94_algebra_t *a,
                   const cos_v94_mv_t u, const cos_v94_mv_t v,
                   cos_v94_mv_t out);

/* One equivariant layer: y = clamp(u*v + bias). */
void cos_v94_layer(const cos_v94_algebra_t *a,
                   const cos_v94_mv_t u, const cos_v94_mv_t v,
                   const cos_v94_mv_t bias,
                   cos_v94_mv_t y);

uint32_t cos_v94_ok(const cos_v94_algebra_t *a);
uint32_t cos_v94_compose_decision(uint32_t v93_composed_ok,
                                  uint32_t v94_ok);

#ifdef __cplusplus
}
#endif

#endif /* COS_V94_CLIFFORD_H */
