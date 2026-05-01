/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/*
 * σ-Swarm — σ-weighted consensus across N model responses (v305 live).
 *
 * One pipeline is good; many are better.  When we have multiple
 * models answer the same query (BitNet + Claude + GPT + DeepSeek,
 * or five BitNet samples at different temperatures), σ tells us
 * which to trust.
 *
 * This primitive operates on (sigma, response_id) tuples — the
 * responses themselves stay in the caller's memory.  Output is the
 * winning index plus the aggregate confidence.
 *
 * Scoring (v0, symmetric with the README examples):
 *     weight_i   = max(0, 1 − σ_i)
 *     winner     = argmax weight_i   (tie → lowest σ → lowest index)
 *     mean_w     = Σ weight_i / N
 *     verdict    = ABSTAIN  if mean_w <  tau_abstain
 *                  ACCEPT   if mean_w >= tau_accept
 *                  else     CONSENSUS (pick winner, σ_swarm = 1−mean_w)
 *
 * Cost-aware escalation ladder (``cos_sigma_swarm_should_escalate``):
 *     given a vector of (sigma_so_far, model_cost_eur), returns a
 *     boolean "add the next model?" decision.  Adds models one at
 *     a time from cheapest → most expensive; stops as soon as the
 *     combined best-σ drops below tau_stop.  Saves API money on
 *     easy queries.
 *
 * Contracts (v0):
 *   1. consensus() over an empty list → verdict = ABSTAIN,
 *      winner_idx = -1, sigma_swarm = 1.0.
 *   2. Ties: same weight → lowest σ → lowest index.
 *   3. All weights zero (every σ ≥ 1) → ABSTAIN.
 *   4. should_escalate(): never escalates past N models; never
 *      escalates if best_sigma already ≤ tau_stop.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef COS_SIGMA_SWARM_H
#define COS_SIGMA_SWARM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float    sigma;           /* per-response σ ∈ [0, 1] */
    float    cost_eur;        /* cost of obtaining this response */
    const char *model_id;     /* optional, not interpreted */
} cos_swarm_response_t;

typedef enum {
    COS_SWARM_ABSTAIN   = 0,
    COS_SWARM_CONSENSUS = 1,   /* pick the winner; moderate confidence */
    COS_SWARM_ACCEPT    = 2,   /* pick the winner; high confidence */
} cos_swarm_verdict_t;

typedef struct {
    cos_swarm_verdict_t verdict;
    int    winner_idx;
    float  winner_sigma;
    float  sigma_swarm;        /* 1 − mean_weight, clamped */
    float  mean_weight;        /* mean max(0, 1 − σ) */
    float  total_cost_eur;     /* Σ cost */
} cos_swarm_decision_t;

cos_swarm_decision_t cos_sigma_swarm_consensus(
    const cos_swarm_response_t *resps, int n,
    float tau_abstain,   /* default 0.30 */
    float tau_accept);   /* default 0.60 */

/* Cost-aware escalation.
 *
 * ``n_used`` is how many models we've already queried; responses
 * are assumed sorted cheapest → most expensive.  Returns true iff
 * adding the next one is worthwhile:
 *   - not exceeding n_total,
 *   - current best σ > tau_stop.
 */
bool cos_sigma_swarm_should_escalate(
    const cos_swarm_response_t *resps, int n_used, int n_total,
    float tau_stop);

int cos_sigma_swarm_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_SIGMA_SWARM_H */
