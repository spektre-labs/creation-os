/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Creation OS v99 — σ-Causal implementation.
 */

#include "causal.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

static inline cos_v99_q16_t q_mul(cos_v99_q16_t a, cos_v99_q16_t b)
{
    return (cos_v99_q16_t)(((int64_t)a * (int64_t)b) >> 16);
}

void cos_v99_scm_init(cos_v99_scm_t *m)
{
    memset(m, 0, sizeof(*m));
    m->sentinel = COS_V99_SENTINEL;
}

void cos_v99_set_edge(cos_v99_scm_t *m,
                      uint32_t j_parent, uint32_t i_child,
                      cos_v99_q16_t w)
{
    if (i_child >= COS_V99_N || j_parent >= COS_V99_N) return;
    if (j_parent >= i_child) return;                    /* enforce DAG */
    m->W[i_child][j_parent] = w;
}

void cos_v99_observe(cos_v99_scm_t *m, const cos_v99_q16_t U[COS_V99_N])
{
    memcpy(m->U, U, sizeof(m->U));
    for (uint32_t i = 0u; i < COS_V99_N; ++i) {
        if (m->intervened[i]) {
            m->X[i] = m->do_val[i];
            continue;
        }
        cos_v99_q16_t acc = m->U[i];
        for (uint32_t j = 0u; j < i; ++j) {
            acc = (cos_v99_q16_t)((int32_t)acc + (int32_t)q_mul(m->W[i][j], m->X[j]));
        }
        m->X[i] = acc;
    }
}

void cos_v99_do(cos_v99_scm_t *m, uint32_t k, cos_v99_q16_t value)
{
    if (k >= COS_V99_N) return;
    m->intervened[k] = 1u;
    m->do_val[k]     = value;
    cos_v99_observe(m, m->U);                          /* re-propagate */
}

void cos_v99_undo_do(cos_v99_scm_t *m, uint32_t k)
{
    if (k >= COS_V99_N) return;
    m->intervened[k] = 0u;
    m->do_val[k]     = 0;
    cos_v99_observe(m, m->U);
}

void cos_v99_clear_interventions(cos_v99_scm_t *m)
{
    for (uint32_t k = 0u; k < COS_V99_N; ++k) {
        m->intervened[k] = 0u;
        m->do_val[k]     = 0;
    }
    cos_v99_observe(m, m->U);
}

cos_v99_q16_t cos_v99_ate(cos_v99_scm_t *m,
                          uint32_t cause, cos_v99_q16_t hi, cos_v99_q16_t lo,
                          uint32_t effect,
                          const cos_v99_q16_t U[COS_V99_N])
{
    if (cause >= COS_V99_N || effect >= COS_V99_N) return 0;

    /* Save state, perform do() ± observation, restore. */
    uint32_t  saved_int[COS_V99_N];
    cos_v99_q16_t saved_do[COS_V99_N];
    memcpy(saved_int, m->intervened, sizeof(saved_int));
    memcpy(saved_do,  m->do_val,     sizeof(saved_do));

    cos_v99_observe(m, U);
    cos_v99_do(m, cause, hi);
    cos_v99_q16_t y_hi = m->X[effect];
    cos_v99_do(m, cause, lo);
    cos_v99_q16_t y_lo = m->X[effect];

    memcpy(m->intervened, saved_int, sizeof(saved_int));
    memcpy(m->do_val,     saved_do,  sizeof(saved_do));
    cos_v99_observe(m, U);

    return (cos_v99_q16_t)((int32_t)y_hi - (int32_t)y_lo);
}

cos_v99_q16_t cos_v99_counterfactual(cos_v99_scm_t *m,
                                     const cos_v99_q16_t U[COS_V99_N],
                                     uint32_t cause,
                                     cos_v99_q16_t intervention_value,
                                     uint32_t effect)
{
    if (cause >= COS_V99_N || effect >= COS_V99_N) return 0;

    uint32_t  saved_int[COS_V99_N];
    cos_v99_q16_t saved_do[COS_V99_N];
    memcpy(saved_int, m->intervened, sizeof(saved_int));
    memcpy(saved_do,  m->do_val,     sizeof(saved_do));

    cos_v99_observe(m, U);                             /* factual pass  */
    cos_v99_do(m, cause, intervention_value);          /* twin with same U */
    cos_v99_q16_t y_cf = m->X[effect];

    memcpy(m->intervened, saved_int, sizeof(saved_int));
    memcpy(m->do_val,     saved_do,  sizeof(saved_do));
    cos_v99_observe(m, U);

    return y_cf;
}

/* Back-door validity for a linear DAG:
 *   - Z must not contain any descendant of `cause`;
 *   - Z must contain every non-descendant ancestor of `cause` that is
 *     also an ancestor of `effect`.
 * The second condition is equivalent, in a DAG, to "Z blocks every
 * back-door path from cause to effect". */
static uint32_t is_ancestor(const cos_v99_scm_t *m, uint32_t a, uint32_t b)
{
    /* Is `a` an ancestor of `b` in this DAG?  BFS on parents-of-b. */
    uint32_t visited = (1u << b);
    uint32_t frontier = (1u << b);
    while (frontier) {
        uint32_t next = 0u;
        for (uint32_t v = 0u; v < COS_V99_N; ++v) {
            if (!(frontier & (1u << v))) continue;
            for (uint32_t j = 0u; j < v; ++j) {
                if (m->W[v][j] != 0 && !(visited & (1u << j))) {
                    visited  |= (1u << j);
                    next     |= (1u << j);
                }
            }
        }
        frontier = next;
    }
    return ((visited & (1u << a)) && a != b) ? 1u : 0u;
}

uint32_t cos_v99_backdoor_valid(const cos_v99_scm_t *m,
                                uint32_t cause, uint32_t effect,
                                uint32_t z_mask)
{
    if (cause >= COS_V99_N || effect >= COS_V99_N) return 0u;

    /* 1. No node in Z is a descendant of `cause`. */
    for (uint32_t v = 0u; v < COS_V99_N; ++v) {
        if (z_mask & (1u << v)) {
            if (v == cause || v == effect) return 0u;
            if (is_ancestor(m, cause, v))  return 0u;
        }
    }

    /* 2. Every node that is a parent of `cause` AND an ancestor of
     *    `effect` is in Z.  (Sufficient for linear DAGs.) */
    for (uint32_t j = 0u; j < cause; ++j) {
        if (m->W[cause][j] == 0) continue;
        if (is_ancestor(m, j, effect) || j == effect) {
            if (!(z_mask & (1u << j))) return 0u;
        }
    }
    return 1u;
}

uint32_t cos_v99_ok(const cos_v99_scm_t *m)
{
    uint32_t sent = (m->sentinel == COS_V99_SENTINEL) ? 1u : 0u;
    /* Upper triangle must be zero (DAG). */
    uint32_t dag = 1u;
    for (uint32_t i = 0u; i < COS_V99_N; ++i) {
        for (uint32_t j = i; j < COS_V99_N; ++j) {
            if (m->W[i][j] != 0) dag = 0u;
        }
    }
    return sent & dag;
}

uint32_t cos_v99_compose_decision(uint32_t v98_composed_ok, uint32_t v99_ok)
{
    return v98_composed_ok & v99_ok;
}
