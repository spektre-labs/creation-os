/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Creation OS v83 — σ-Agentic implementation.
 *
 * Integer-only phase-space rollout, Q16.16 surprise, Q16.16 energy.
 * Branchless hot path.  Receipts are SHAKE-256 from v81.
 */

#include "agentic.h"
#include "../v81/lattice.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

static inline cos_v83_q16_t q_abs(cos_v83_q16_t x) {
    /* Branchless absolute value on int32, wrap-safe at INT32_MIN. */
    uint32_t ux = (uint32_t)x;
    uint32_t m  = (uint32_t)(x >> 31);      /* 0 or 0xFFFFFFFFu */
    return (cos_v83_q16_t)((ux ^ m) - m);
}

/* ------------------------------------------------------------------ */
/*  Plan-step: 32 opcodes, each deterministic Q16.16 update.           */
/* ------------------------------------------------------------------ */
uint32_t cos_v83_plan_step(cos_v83_state_t *s,
                           const cos_v83_plan_cell_t *cell)
{
    uint32_t op = cell->op & 0x1Fu;
    int32_t  p  = cell->param;

    /* All updates in Q16.16; use integer arithmetic only.  Each
     * opcode's effect is deliberately small so that accumulated
     * surprise stays meaningful. */
    /* Wrap-safe arithmetic: cast to uint32_t for add/sub/mul so that
     * signed overflow is not UB (we need two's-complement wrap). */
    uint32_t ux = (uint32_t)s->x;
    uint32_t uv = (uint32_t)s->v;
    uint32_t up = (uint32_t)p;
    switch (op) {
    case 0:  s->x = (int32_t)(ux + up);                        break;
    case 1:  s->v = (int32_t)(uv + up);                        break;
    case 2:  s->x = (int32_t)(ux + (uint32_t)(s->v >> 4));     break;
    case 3:  s->v = (int32_t)(uv - (uint32_t)(s->x >> 4));     break;
    case 4:  s->x = (int32_t)((ux * 15u) >> 4);                break;
    case 5:  s->v = (int32_t)((uv * 15u) >> 4);                break;
    case 6:  s->x = (int32_t)(ux ^ (up & 0xFFFFu));            break;
    case 7:  s->v = (int32_t)(uv ^ (up & 0xFFFFu));            break;
    case 8:  s->x = (int32_t)(ux + (uint32_t)(p >> 8));        break;
    case 9:  s->v = (int32_t)(uv + (uint32_t)(p >> 8));        break;
    case 10: s->x = (int32_t)(0u - ux);                        break;
    case 11: s->v = (int32_t)(0u - uv);                        break;
    case 12: { int32_t t = s->x; s->x = s->v; s->v = t; }      break;
    case 13: s->x = (int32_t)(ux + (uint32_t)(s->v >> 8));     break;
    case 14: s->v = (int32_t)(uv + (uint32_t)(s->x >> 8));     break;
    case 15: s->x = (int32_t)((uint32_t)(s->x >> 1) + (uint32_t)(p >> 1)); break;
    default:
        /* opcodes 16..31 are NOPs that consume the param slot; this
         * keeps the op-space extensible without invalidating plans. */
        (void)p;
        break;
    }
    return 1u;
}

/* ------------------------------------------------------------------ */
/*  Init.                                                              */
/* ------------------------------------------------------------------ */
void cos_v83_agent_init(cos_v83_agent_t *a, cos_v83_q16_t energy_budget)
{
    memset(a, 0, sizeof(*a));
    a->energy_budget = energy_budget;
    a->sentinel      = COS_V83_SENTINEL;
    /* Seed the receipt with SHAKE256("cos-v83"). */
    static const uint8_t tag[7] = {'c','o','s','-','v','8','3'};
    cos_v81_xof_t x;
    cos_v81_shake256_init(&x);
    cos_v81_shake_absorb(&x, tag, sizeof(tag));
    cos_v81_shake_finalize(&x);
    cos_v81_shake_squeeze(&x, a->receipt, COS_V83_RECEIPT_BYTES);
}

/* ------------------------------------------------------------------ */
/*  Receipt update (chain).                                            */
/* ------------------------------------------------------------------ */
static void receipt_chain(uint8_t out[32], const uint8_t in[32],
                          const cos_v83_state_t *start,
                          const cos_v83_state_t *end,
                          cos_v83_q16_t surprise,
                          uint32_t accepted)
{
    uint8_t buf[32 + 16 + 4 + 4];
    memcpy(buf, in, 32);
    memcpy(buf + 32, start, 8);
    memcpy(buf + 32 + 8, end, 8);
    memcpy(buf + 32 + 16, &surprise, 4);
    memcpy(buf + 32 + 16 + 4, &accepted, 4);

    cos_v81_xof_t x;
    cos_v81_shake256_init(&x);
    cos_v81_shake_absorb(&x, buf, sizeof(buf));
    cos_v81_shake_finalize(&x);
    cos_v81_shake_squeeze(&x, out, 32);
}

/* ------------------------------------------------------------------ */
/*  Prior expectation: the average of existing priors' end - start.    */
/*  Empty memory -> expectation (0, 0).                                */
/* ------------------------------------------------------------------ */
static void expected_delta(const cos_v83_agent_t *a,
                           cos_v83_state_t *exp_out)
{
    exp_out->x = 0;
    exp_out->v = 0;
    if (a->count == 0) return;

    int64_t sum_dx = 0, sum_dv = 0;
    for (uint32_t i = 0; i < a->count; ++i) {
        sum_dx += (int64_t)a->prior[i].end.x - (int64_t)a->prior[i].start.x;
        sum_dv += (int64_t)a->prior[i].end.v - (int64_t)a->prior[i].start.v;
    }
    exp_out->x = (cos_v83_q16_t)(sum_dx / (int64_t)a->count);
    exp_out->v = (cos_v83_q16_t)(sum_dv / (int64_t)a->count);
}

/* ------------------------------------------------------------------ */
/*  One full step.                                                     */
/* ------------------------------------------------------------------ */
uint32_t cos_v83_step(cos_v83_agent_t *a,
                      const cos_v83_plan_cell_t *plan,
                      uint32_t plan_len)
{
    if (a->sentinel != COS_V83_SENTINEL) {
        ++a->invariant_violations;
        return 0u;
    }
    if (plan_len > COS_V83_PLAN_MAX) {
        ++a->invariant_violations;
        return 0u;
    }

    /* Snapshot for possible rollback. */
    a->snapshot = a->live;

    /* ROLL: execute the plan on the live state. */
    cos_v83_state_t start = a->live;
    for (uint32_t i = 0; i < plan_len; ++i) {
        cos_v83_plan_step(&a->live, &plan[i]);
    }
    cos_v83_state_t end = a->live;

    /* SURPRISE: |expected - actual| in x and v, summed. */
    cos_v83_state_t exp_delta;
    expected_delta(a, &exp_delta);

    /* Wrap-safe diffs: perform subtraction in uint32_t to avoid signed
     * overflow UB on large perturbations; the result is then interpreted
     * as a signed Q16.16 delta. */
    cos_v83_q16_t actual_dx = (cos_v83_q16_t)((uint32_t)end.x - (uint32_t)start.x);
    cos_v83_q16_t actual_dv = (cos_v83_q16_t)((uint32_t)end.v - (uint32_t)start.v);
    cos_v83_q16_t s_x = q_abs((cos_v83_q16_t)((uint32_t)actual_dx - (uint32_t)exp_delta.x));
    cos_v83_q16_t s_v = q_abs((cos_v83_q16_t)((uint32_t)actual_dv - (uint32_t)exp_delta.v));
    cos_v83_q16_t surprise = (cos_v83_q16_t)((uint32_t)s_x + (uint32_t)s_v);

    /* ENERGY: E_new = E + (surprise >> 3) — wrap-safe.  We compute in
     * uint32 to avoid signed-overflow UB, then reject if the result
     * wrapped (would become negative as int32) OR exceeds the budget.
     * Either condition means the plan is far outside the prior and must
     * be rolled back. */
    uint32_t delta_e = (uint32_t)(surprise >> 3);
    uint32_t new_energy_u = (uint32_t)a->energy + delta_e;
    cos_v83_q16_t new_energy = (cos_v83_q16_t)new_energy_u;
    uint32_t wrapped = (new_energy < 0) ? 1u : 0u;
    uint32_t over    = (new_energy > a->energy_budget) ? 1u : 0u;
    uint32_t accepted = (wrapped | over) ? 0u : 1u;

    if (accepted) {
        /* Commit: add prior, advance energy, extend receipt. */
        cos_v83_prior_t *p = &a->prior[a->head];
        p->start    = start;
        p->end      = end;
        p->surprise = surprise;
        a->head = (a->head + 1u) % COS_V83_MEMORY_SLOTS;
        if (a->count < COS_V83_MEMORY_SLOTS) ++a->count;
        a->energy = new_energy;
        ++a->accepts;
    } else {
        /* Reject: rollback. */
        a->live = a->snapshot;
        ++a->rejects;
    }

    /* Always chain the receipt (accepted or rejected). */
    uint8_t new_recv[32];
    receipt_chain(new_recv, a->receipt, &start, &end, surprise, accepted);
    memcpy(a->receipt, new_recv, 32);

    return accepted;
}

/* ------------------------------------------------------------------ */
/*  Consolidation: average the last n priors into one.                 */
/* ------------------------------------------------------------------ */
uint32_t cos_v83_consolidate(cos_v83_agent_t *a, uint32_t n)
{
    if (a->sentinel != COS_V83_SENTINEL) { ++a->invariant_violations; return a->count; }
    if (n == 0 || n > a->count) n = a->count;
    if (n == 0) return 0u;

    int64_t sx = 0, sv = 0, ex = 0, ev = 0;
    int64_t ss = 0;
    for (uint32_t i = 0; i < n; ++i) {
        uint32_t idx = (a->head + COS_V83_MEMORY_SLOTS - 1u - i) % COS_V83_MEMORY_SLOTS;
        sx += a->prior[idx].start.x;
        sv += a->prior[idx].start.v;
        ex += a->prior[idx].end.x;
        ev += a->prior[idx].end.v;
        ss += a->prior[idx].surprise;
    }
    cos_v83_prior_t merged;
    merged.start.x  = (cos_v83_q16_t)(sx / (int64_t)n);
    merged.start.v  = (cos_v83_q16_t)(sv / (int64_t)n);
    merged.end.x    = (cos_v83_q16_t)(ex / (int64_t)n);
    merged.end.v    = (cos_v83_q16_t)(ev / (int64_t)n);
    merged.surprise = (cos_v83_q16_t)(ss / (int64_t)n);

    /* Clear the consumed n slots and replace with the merged prior. */
    memset(a->prior, 0, sizeof(a->prior));
    a->prior[0] = merged;
    a->head  = 1u;
    a->count = 1u;

    /* Relax the energy proportionally. */
    if (n > 1u) {
        a->energy = (cos_v83_q16_t)(a->energy / (cos_v83_q16_t)n);
    }
    return a->count;
}

/* ------------------------------------------------------------------ */
/*  Gate + compose.                                                    */
/* ------------------------------------------------------------------ */
uint32_t cos_v83_ok(const cos_v83_agent_t *a)
{
    uint32_t sentinel_ok = (a->sentinel == COS_V83_SENTINEL) ? 1u : 0u;
    uint32_t no_violations = (a->invariant_violations == 0u) ? 1u : 0u;
    uint32_t energy_ok = (a->energy >= 0) ? 1u : 0u;
    return sentinel_ok & no_violations & energy_ok;
}

uint32_t cos_v83_compose_decision(uint32_t v82_composed_ok, uint32_t v83_ok)
{
    return v82_composed_ok & v83_ok;
}
