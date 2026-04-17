/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Creation OS v83 — σ-Agentic (the learner loop).
 *
 * ------------------------------------------------------------------
 *  What v83 is
 * ------------------------------------------------------------------
 *
 * Up through v82 the Creation OS stack is a twenty-two-bit *verifier*:
 * it decides whether to emit, and it streams that decision.  What it
 * does not do is *learn from the emission*.  ARC-AGI-3 (2026)
 * demonstrates that frontier verifier-only systems score below 1 %
 * on tasks that require building an internal model of environment
 * dynamics.  Friston (2010) says: without a free-energy-minimising
 * feedback loop from outcomes back into priors, there is no agent.
 *
 * v83 closes the loop.  Every step is four stages:
 *
 *   1. PLAN   — an integer program of length L is proposed (the
 *      output of v80 σ-Cortex's TTC VM, or any equivalent integer
 *      program).  We represent the plan as a sequence of L cells,
 *      each a (uint32_t opcode, int32_t param).
 *
 *   2. ROLL   — v79 σ-Simulacrum is stepped L times under the plan.
 *      Here we model the simulacrum as a compact Q16.16 phase-space
 *      point (x, v) ∈ Z^2 evolving under a deterministic opcode-
 *      specific update rule.  Read out the post-roll state delta.
 *
 *   3. SURPRISE  — variational free energy.  The *expected* delta
 *      under the prior (the last-known-good trajectory kept in
 *      v83's internal memory) is compared to the *actual* delta
 *      under this roll.  Surprise s = |expected - actual|, in Q16.16.
 *
 *   4. ENERGY   — energy-based refinement (Du et al., 2023 IBL).
 *      The plan's energy E is updated as E_new = E_old + α·s, with
 *      α = 1/8.  If E_new exceeds a budget B, the plan is REJECTED:
 *      v83_ok = 0 for this step, the loop rolls back to the prior
 *      memory snapshot, and the cortex receipt is annotated with
 *      the reject witness.  Otherwise the plan is ACCEPTED: the
 *      memory snapshot advances, v83_ok = 1, and the agent has
 *      *actually learned* (the trajectory-delta is folded into the
 *      prior for the next step).
 *
 * Five primitives:
 *
 *   1. cos_v83_agent_init           — fresh loop state.
 *   2. cos_v83_plan_step            — apply a single opcode to the
 *      phase-space rollout, deterministic.
 *   3. cos_v83_step                 — run one full PLAN→ROLL→
 *      SURPRISE→ENERGY step.  Returns 1 if the plan was accepted
 *      (memory advanced), 0 if rejected (memory rolled back).
 *   4. cos_v83_consolidate          — Mnemos-style sleep: collapse
 *      the last N accepted trajectories into a single prior, and
 *      emit a v72-compatible receipt.  Integer only.
 *   5. cos_v83_ok / cos_v83_compose_decision — the 23-bit gate.
 *
 * ------------------------------------------------------------------
 *  Composed 23-bit branchless decision (extends v82)
 * ------------------------------------------------------------------
 *
 *     cos_v83_compose_decision(v82_composed_ok, v83_ok)
 *         = v82_composed_ok & v83_ok
 *
 * `v83_ok = 1` iff, since the loop was initialised:
 *   - every accepted step decreased (or kept equal) the running
 *     surprise,   -or-  was the first step (cold start),
 *   - every REJECTED step led to a correct rollback to the prior
 *     snapshot (memory invariant preserved),
 *   - the prior-count never goes negative,
 *   - the energy never exceeds (budget + α · N) for N steps,
 *   - the receipt chain is intact.
 *
 * Hardware discipline: integer, branchless on the hot path, libc-
 * only, no malloc on the hot path.
 */

#ifndef COS_V83_AGENTIC_H
#define COS_V83_AGENTIC_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V83_PLAN_MAX       64u     /* max plan length */
#define COS_V83_MEMORY_SLOTS   16u     /* ring-buffer of accepted priors */
#define COS_V83_RECEIPT_BYTES  32u
#define COS_V83_SENTINEL       0xA515CEDEu

/* Q16.16 typedef (same as v79 / v80). */
typedef int32_t cos_v83_q16_t;

#define COS_V83_Q16_ONE    (1 << 16)

/* A plan cell. */
typedef struct {
    uint32_t op;       /* 0..31, v83-internal opcode */
    int32_t  param;    /* signed parameter */
} cos_v83_plan_cell_t;

/* A phase-space point (x, v) in Q16.16. */
typedef struct {
    cos_v83_q16_t x;
    cos_v83_q16_t v;
} cos_v83_state_t;

/* A single prior: the last-accepted trajectory delta. */
typedef struct {
    cos_v83_state_t start;
    cos_v83_state_t end;
    cos_v83_q16_t   surprise;    /* Q16.16 */
} cos_v83_prior_t;

typedef struct {
    /* Ring buffer of priors. */
    cos_v83_prior_t prior[COS_V83_MEMORY_SLOTS];
    uint32_t        head;                 /* next write slot */
    uint32_t        count;                /* number of filled slots */

    /* Working state. */
    cos_v83_state_t live;                 /* current phase-space point */
    cos_v83_state_t snapshot;             /* rollback target */

    /* Energy accounting. */
    cos_v83_q16_t   energy;               /* Q16.16, non-negative */
    cos_v83_q16_t   energy_budget;        /* Q16.16 cap */

    /* Running receipt (v72-compatible BLAKE2b-style 32-byte digest
     * via v81 SHAKE-256). */
    uint8_t         receipt[COS_V83_RECEIPT_BYTES];

    /* Counters. */
    uint32_t        accepts;
    uint32_t        rejects;
    uint32_t        invariant_violations;
    uint32_t        sentinel;
} cos_v83_agent_t;

/* Initialise a fresh loop. */
void cos_v83_agent_init(cos_v83_agent_t *a,
                        cos_v83_q16_t energy_budget);

/* Apply a single opcode to (x, v) deterministically.  Returns 1 if
 * the opcode was valid. */
uint32_t cos_v83_plan_step(cos_v83_state_t *s,
                           const cos_v83_plan_cell_t *cell);

/* Run one full PLAN → ROLL → SURPRISE → ENERGY step.
 * Returns 1 if the plan was accepted, 0 if rejected.  On reject,
 * `a->live` is restored to `a->snapshot`. */
uint32_t cos_v83_step(cos_v83_agent_t *a,
                      const cos_v83_plan_cell_t *plan,
                      uint32_t plan_len);

/* Mnemos-style consolidation: collapse the last `n` accepted priors
 * into a single averaged prior.  Returns the new prior count.  */
uint32_t cos_v83_consolidate(cos_v83_agent_t *a, uint32_t n);

/* Gate + compose. */
uint32_t cos_v83_ok(const cos_v83_agent_t *a);
uint32_t cos_v83_compose_decision(uint32_t v82_composed_ok,
                                  uint32_t v83_ok);

#ifdef __cplusplus
}
#endif

#endif /* COS_V83_AGENTIC_H */
