/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Creation OS v99 — σ-Causal (structural causal model + do-calculus
 * + back-door adjustment + counterfactual twin graph).
 *
 * ------------------------------------------------------------------
 *  What v99 is
 * ------------------------------------------------------------------
 *
 * An integer-only realisation of Pearl's structural-causal-model (SCM)
 * machinery, compact enough for full run-time falsification.
 *
 * Let X = (X_0, X_1, …, X_{N-1}) be the variables of a DAG.  Each
 * variable has a linear structural equation with Q16.16 weights:
 *
 *     X_i  =  Σ_{j ∈ pa(i)} W[i][j] · X_j  +  U_i               (i = 0..N-1)
 *
 * where U_i is an exogenous noise term treated as a known input.  The
 * DAG is given as a strict upper-triangular adjacency matrix so
 * topological order is 0, 1, 2, …, N-1 by construction.
 *
 * Primitives:
 *
 *   1. cos_v99_scm_init       zero DAG, sentinel, canonical topo order
 *   2. cos_v99_set_edge       W[i][j] with i > j  (j is a parent of i)
 *   3. cos_v99_observe        forward pass given exogenous noise U
 *   4. cos_v99_do             intervention do(X_k = value); re-propagate
 *   5. cos_v99_ate            E[X_Y | do(X_X = hi)] − E[X_Y | do(X_X = lo)]
 *   6. cos_v99_counterfactual twin graph with shared noise + do-query
 *   7. cos_v99_backdoor_valid minimal back-door adjustment check
 *   8. cos_v99_ok / compose   38→39-bit verdict
 *
 * Invariants checked at runtime:
 *   • Adjacency strictly upper-triangular  → acyclic
 *   • Intervention severs incoming edges of the intervened node
 *   • Counterfactual with ΔU = 0 equals the factual outcome
 *   • ATE is linear in the path-coefficient product along the directed
 *     X → Y path for the uncovered linear case (gold standard)
 *   • sentinel intact
 *
 * ------------------------------------------------------------------
 *  Composed 39-bit branchless decision (extends v98)
 * ------------------------------------------------------------------
 *
 *     cos_v99_compose_decision(v98_composed_ok, v99_ok)
 *         = v98_composed_ok & v99_ok
 */

#ifndef COS_V99_CAUSAL_H
#define COS_V99_CAUSAL_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V99_SENTINEL   0x99CA05A1u
#define COS_V99_N          6u               /* variables */
#define COS_V99_Q1         65536            /* 1.0 Q16.16 */

typedef int32_t cos_v99_q16_t;

typedef struct {
    /* W[i][j] with j < i is the structural coefficient from parent j
     * to child i.  Upper triangle (j ≥ i) is zero. */
    cos_v99_q16_t W     [COS_V99_N][COS_V99_N];
    cos_v99_q16_t X     [COS_V99_N];                 /* current variable values  */
    cos_v99_q16_t U     [COS_V99_N];                 /* last exogenous noise      */
    uint32_t      intervened[COS_V99_N];             /* 1 if this node is under do() */
    cos_v99_q16_t do_val[COS_V99_N];
    uint32_t      sentinel;
} cos_v99_scm_t;

void cos_v99_scm_init(cos_v99_scm_t *m);

/* Add a directed edge j → i (i is the child; requires j < i). */
void cos_v99_set_edge(cos_v99_scm_t *m,
                      uint32_t j_parent, uint32_t i_child,
                      cos_v99_q16_t w);

/* Forward-evaluate all variables in topological order given U. */
void cos_v99_observe(cos_v99_scm_t *m, const cos_v99_q16_t U[COS_V99_N]);

/* Intervention: set X_k = value and re-propagate.  A subsequent
 * observe() respects the intervention; cos_v99_undo_do clears it. */
void cos_v99_do(cos_v99_scm_t *m, uint32_t k, cos_v99_q16_t value);
void cos_v99_undo_do(cos_v99_scm_t *m, uint32_t k);
void cos_v99_clear_interventions(cos_v99_scm_t *m);

/* Average-treatment-effect between do(X_X = hi) and do(X_X = lo). */
cos_v99_q16_t cos_v99_ate(cos_v99_scm_t *m,
                          uint32_t cause, cos_v99_q16_t hi, cos_v99_q16_t lo,
                          uint32_t effect,
                          const cos_v99_q16_t U[COS_V99_N]);

/* Counterfactual: observe U, then query X_effect under do(cause=val).
 * The intervention is temporary — state is restored on exit. */
cos_v99_q16_t cos_v99_counterfactual(cos_v99_scm_t *m,
                                     const cos_v99_q16_t U[COS_V99_N],
                                     uint32_t cause,
                                     cos_v99_q16_t intervention_value,
                                     uint32_t effect);

/* Minimal back-door criterion: returns 1 iff the set Z (given as a
 * bitmask over {0, …, N-1}) blocks every back-door path from cause
 * to effect in this linear DAG.  A cleaner, stricter form: Z contains
 * every non-descendant parent of `cause` that lies on at least one
 * path to `effect`. */
uint32_t cos_v99_backdoor_valid(const cos_v99_scm_t *m,
                                uint32_t cause, uint32_t effect,
                                uint32_t z_mask);

uint32_t cos_v99_ok(const cos_v99_scm_t *m);
uint32_t cos_v99_compose_decision(uint32_t v98_composed_ok,
                                  uint32_t v99_ok);

#ifdef __cplusplus
}
#endif

#endif /* COS_V99_CAUSAL_H */
