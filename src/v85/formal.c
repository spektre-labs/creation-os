/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Creation OS v85 — σ-Formal implementation.
 *
 * Branchless-ish hot path.  Each observe is O(P) where P is the
 * registered-predicate count.  No malloc; fixed-size state.
 */

#include "formal.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

void cos_v85_checker_init(cos_v85_checker_t *c)
{
    memset(c, 0, sizeof(*c));
    c->sentinel = COS_V85_SENTINEL;
}

static uint32_t register_pred(cos_v85_checker_t *c,
                              cos_v85_pred_kind_t kind,
                              cos_v85_pred_t p,
                              cos_v85_pred_t q)
{
    if (c->sentinel != COS_V85_SENTINEL) return UINT32_MAX;
    if (c->pred_count >= COS_V85_MAX_PREDS) return UINT32_MAX;
    uint32_t id = c->pred_count++;
    cos_v85_pred_rec_t *r = &c->preds[id];
    memset(r, 0, sizeof(*r));
    r->kind = kind;
    r->p = p;
    r->q = q;
    return id;
}

uint32_t cos_v85_register_always(cos_v85_checker_t *c, cos_v85_pred_t p)
{
    return register_pred(c, COS_V85_PRED_ALWAYS, p, NULL);
}

uint32_t cos_v85_register_eventually(cos_v85_checker_t *c, cos_v85_pred_t p)
{
    return register_pred(c, COS_V85_PRED_EVENTUALLY, p, NULL);
}

uint32_t cos_v85_register_responds(cos_v85_checker_t *c,
                                   cos_v85_pred_t p, cos_v85_pred_t q)
{
    return register_pred(c, COS_V85_PRED_RESPONDS, p, q);
}

uint32_t cos_v85_observe(cos_v85_checker_t *c, uint64_t state)
{
    if (c->sentinel != COS_V85_SENTINEL) return 0u;
    c->last_state = state;
    ++c->step;

    for (uint32_t i = 0; i < c->pred_count; ++i) {
        cos_v85_pred_rec_t *r = &c->preds[i];
        uint32_t p_val = r->p ? r->p(state) : 0u;
        switch (r->kind) {
        case COS_V85_PRED_ALWAYS:
            if (!p_val && !r->violated) {
                r->violated = 1u;
                r->witness  = state;
            }
            break;
        case COS_V85_PRED_EVENTUALLY:
            if (p_val) {
                r->fired = 1u;
            }
            break;
        case COS_V85_PRED_RESPONDS: {
            uint32_t q_val = r->q ? r->q(state) : 0u;
            if (p_val) r->pending = 1u;
            if (r->pending && q_val) {
                r->resolved = 1u;
                r->pending  = 0u;
            }
        } break;
        default:
            /* Unknown kind: treat as immediate fail. */
            r->violated = 1u;
            r->witness  = state;
            break;
        }
    }

    return cos_v85_status(c);
}

uint32_t cos_v85_status(const cos_v85_checker_t *c)
{
    uint32_t always_ok = 1u;
    uint32_t eventually_ok = 1u;
    uint32_t responds_ok = 1u;
    uint32_t has_eventually = 0u;
    uint32_t has_responds = 0u;

    for (uint32_t i = 0; i < c->pred_count; ++i) {
        const cos_v85_pred_rec_t *r = &c->preds[i];
        switch (r->kind) {
        case COS_V85_PRED_ALWAYS:
            if (r->violated) always_ok = 0u;
            break;
        case COS_V85_PRED_EVENTUALLY:
            has_eventually = 1u;
            if (!r->fired) eventually_ok = 0u;
            break;
        case COS_V85_PRED_RESPONDS:
            has_responds = 1u;
            /* A responds predicate must (a) have at least one
             * resolved pair, AND (b) not be currently pending at
             * the end of the trace. */
            if (!r->resolved || r->pending) responds_ok = 0u;
            break;
        default:
            break;
        }
    }

    /* If no eventually/responds are registered, treat their bits as ok. */
    if (!has_eventually) eventually_ok = 1u;
    if (!has_responds)   responds_ok = 1u;

    return (always_ok      & 1u)
         | ((eventually_ok & 1u) << 1)
         | ((responds_ok   & 1u) << 2);
}

uint64_t cos_v85_witness(const cos_v85_checker_t *c, uint32_t pred_id)
{
    if (pred_id >= c->pred_count) return 0ULL;
    return c->preds[pred_id].witness;
}

uint32_t cos_v85_ok(const cos_v85_checker_t *c)
{
    return (cos_v85_status(c) == 0x7u) ? 1u : 0u;
}

uint32_t cos_v85_compose_decision(uint32_t v84_composed_ok, uint32_t v85_ok)
{
    return v84_composed_ok & v85_ok;
}
