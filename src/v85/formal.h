/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Creation OS v85 — σ-Formal (runtime TLA-style invariant checker).
 *
 * ------------------------------------------------------------------
 *  What v85 is
 * ------------------------------------------------------------------
 *
 * The previous twenty-four kernels each ship runtime-checked
 * invariants inside their own self-tests.  v85 takes the step
 * TLA+ (Lamport 1994) made mandatory for distributed systems:
 * a *single* checker that encodes the invariants as temporal
 * predicates and tracks them across every state step.
 *
 * Three classes of temporal predicate:
 *
 *   - ALWAYS(P)           — P must hold at every observed state.
 *   - EVENTUALLY(P)       — P must hold at some observed state
 *                           before the trace ends.
 *   - RESPONDS(P, Q)      — every time P holds, some later state
 *                           in the same trace has Q hold (the
 *                           "leads-to" operator, P ~> Q).
 *
 * Each predicate takes a pointer to the current state word (a
 * uint64 packed with the 24 per-kernel ok-bits plus a scratch
 * register) and returns 0/1.
 *
 * Five primitives:
 *
 *   1. cos_v85_checker_init         — fresh checker with a bounded
 *      number of registered predicates.
 *   2. cos_v85_register_always      — add an ALWAYS predicate.
 *   3. cos_v85_register_eventually  — add an EVENTUALLY predicate.
 *   4. cos_v85_register_responds    — add a RESPONDS (P ~> Q).
 *   5. cos_v85_observe              — feed one state word into the
 *      checker.  Returns the current 3-bit verdict:
 *         bit 0 = all ALWAYS predicates still hold,
 *         bit 1 = all EVENTUALLY predicates have fired,
 *         bit 2 = all RESPONDS predicates have been resolved.
 *
 * ------------------------------------------------------------------
 *  Composed 25-bit branchless decision (extends v84)
 * ------------------------------------------------------------------
 *
 *     cos_v85_compose_decision(v84_composed_ok, v85_ok)
 *         = v84_composed_ok & v85_ok
 *
 * `v85_ok = 1` iff every registered ALWAYS predicate still holds
 * *and* every registered EVENTUALLY predicate has fired at least
 * once *and* every registered RESPONDS predicate has been resolved.
 *
 * ------------------------------------------------------------------
 *  A machine-readable TLA+ spec of the composed-decision property
 *  lives in `docs/formal/composed_decision.tla` — the one-liner is:
 *
 *     COMPOSED == /\ \A i \in 60..84 : kernel_ok[i]
 *                 /\ <>stream_halts_on_flip
 *                 /\ agent_rejects => memory_rolls_back
 *
 *  v85 runtime-checks exactly that spec.  When the invariant fires
 *  false the checker returns the witness state word so the user can
 *  diagnose the minimal counter-example.
 */

#ifndef COS_V85_FORMAL_H
#define COS_V85_FORMAL_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V85_MAX_PREDS     32u
#define COS_V85_SENTINEL      0x7F04CA11u

typedef uint32_t (*cos_v85_pred_t)(uint64_t state);

typedef enum {
    COS_V85_PRED_ALWAYS     = 1u,
    COS_V85_PRED_EVENTUALLY = 2u,
    COS_V85_PRED_RESPONDS   = 3u,   /* p must be followed by q */
} cos_v85_pred_kind_t;

typedef struct {
    cos_v85_pred_kind_t kind;
    cos_v85_pred_t      p;
    cos_v85_pred_t      q;          /* for RESPONDS only */
    uint32_t            fired;      /* for EVENTUALLY */
    uint32_t            resolved;   /* for RESPONDS */
    uint32_t            pending;    /* for RESPONDS: p has triggered, q not yet */
    uint64_t            witness;    /* last state for which predicate failed */
    uint32_t            violated;   /* ALWAYS: 1 if ever false */
} cos_v85_pred_rec_t;

typedef struct {
    cos_v85_pred_rec_t  preds[COS_V85_MAX_PREDS];
    uint32_t            pred_count;
    uint64_t            last_state;
    uint32_t            step;
    uint32_t            sentinel;
} cos_v85_checker_t;

/* Init. */
void cos_v85_checker_init(cos_v85_checker_t *c);

/* Register predicates.  Return the predicate id, or UINT32_MAX on
 * failure (no room). */
uint32_t cos_v85_register_always(cos_v85_checker_t *c, cos_v85_pred_t p);
uint32_t cos_v85_register_eventually(cos_v85_checker_t *c, cos_v85_pred_t p);
uint32_t cos_v85_register_responds(cos_v85_checker_t *c,
                                   cos_v85_pred_t p, cos_v85_pred_t q);

/* Observe one state word.  Returns the 3-bit verdict. */
uint32_t cos_v85_observe(cos_v85_checker_t *c, uint64_t state);

/* Current status.  Returns the 3-bit verdict without observing. */
uint32_t cos_v85_status(const cos_v85_checker_t *c);

/* Witness access. */
uint64_t cos_v85_witness(const cos_v85_checker_t *c, uint32_t pred_id);

/* Gate + compose. */
uint32_t cos_v85_ok(const cos_v85_checker_t *c);
uint32_t cos_v85_compose_decision(uint32_t v84_composed_ok,
                                  uint32_t v85_ok);

#ifdef __cplusplus
}
#endif

#endif /* COS_V85_FORMAL_H */
