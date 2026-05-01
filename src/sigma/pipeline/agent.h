/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/*
 * σ-Agent — OODA + Reflect with σ-modulated autonomy.
 *
 * The pipeline so far is reactive: user asks → pipeline answers.
 * An autonomous agent observes, decides, and acts on its own — but
 * an agent that acts unconditionally on top of an LLM is dangerous.
 *
 * σ-Agent reuses the loop John Boyd would recognise:
 *
 *     OBSERVE  → ORIENT  → DECIDE  → ACT  → REFLECT  → OBSERVE …
 *
 * with one twist: the agent's effective autonomy is dynamically
 * scaled by σ.  High σ (uncertain) shrinks autonomy; the gate
 * forces a CONFIRM or BLOCK.  Low σ (confident) restores full
 * autonomy and the gate ALLOWS the action through.
 *
 *     effective_autonomy = base_autonomy · (1 − σ)
 *
 *     gate(action_class, effective_autonomy)
 *         | effective_autonomy ≥ required_for(action) → ALLOW
 *         | effective_autonomy ≥ confirm_threshold(action) → CONFIRM
 *         | otherwise                                       → BLOCK
 *
 * Action classes carry a `min_autonomy` requirement (e.g. read-
 * file = 0.30, write-file = 0.60, network-call = 0.80, irreversible
 * = 0.95).  The gate is the line between "the agent can act"
 * and "the agent must ask".
 *
 * The Reflect phase records (sigma_predicted, sigma_observed) so
 * later passes can recalibrate via sigma_live (P15) — the agent
 * learns when its own confidence was wrong.
 *
 * Contracts (v0):
 *   1. init() rejects autonomy ∉ [0, 1] and confirm_margin ≤ 0.
 *   2. advance() walks the cycle in canonical order and asserts the
 *      caller did not skip phases.
 *   3. gate() never panics; on NULL or invalid σ it returns BLOCK.
 *   4. The agent state is plain old data: 4 ints + 4 floats + a
 *      tiny per-action stats array — copyable, comparable.
 *   5. Self-test covers (a) the full OODA cycle, (b) low-σ →
 *      ALLOW, (c) high-σ → BLOCK, (d) tie-break to CONFIRM, (e)
 *      reflect tracks predicted vs observed σ and computes
 *      lifetime calibration error.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef COS_SIGMA_AGENT_H
#define COS_SIGMA_AGENT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    COS_OODA_OBSERVE = 0,
    COS_OODA_ORIENT  = 1,
    COS_OODA_DECIDE  = 2,
    COS_OODA_ACT     = 3,
    COS_OODA_REFLECT = 4,
} cos_ooda_phase_t;

typedef enum {
    COS_AGENT_BLOCK   = 0,
    COS_AGENT_CONFIRM = 1,
    COS_AGENT_ALLOW   = 2,
} cos_agent_decision_t;

/* Canonical action classes.  Callers can layer their own; these
 * exist so check-sigma-agent can pin a known table. */
typedef enum {
    COS_ACT_READ          = 0,    /* min_autonomy 0.30 */
    COS_ACT_WRITE         = 1,    /* min_autonomy 0.60 */
    COS_ACT_NETWORK       = 2,    /* min_autonomy 0.80 */
    COS_ACT_IRREVERSIBLE  = 3,    /* min_autonomy 0.95 */
    COS_ACT__N            = 4,
} cos_action_class_t;

typedef struct {
    cos_ooda_phase_t phase;
    float            base_autonomy;     /* 0..1 */
    float            confirm_margin;    /* CONFIRM band width      */
    float            last_sigma;        /* σ from latest OBSERVE   */
    /* Reflect bookkeeping. */
    int              n_cycles;
    double           sum_sigma_pred;
    double           sum_sigma_obs;
    double           sum_abs_err;
} cos_sigma_agent_t;

int   cos_sigma_agent_init(cos_sigma_agent_t *a,
                           float base_autonomy,
                           float confirm_margin);

/* Advance to the next OODA phase; returns 0 on success or -1 if
 * the caller is out of sequence. */
int   cos_sigma_agent_advance(cos_sigma_agent_t *a,
                              cos_ooda_phase_t expected);

/* Gate an action class given the *current* σ.  Returns ALLOW,
 * CONFIRM or BLOCK.  Stateless beyond the agent's `base_autonomy`. */
cos_agent_decision_t
      cos_sigma_agent_gate(const cos_sigma_agent_t *a,
                           cos_action_class_t cls,
                           float sigma);

/* Min effective-autonomy required for each canonical class. */
float cos_sigma_agent_min_autonomy(cos_action_class_t cls);

/* Reflect-phase update: feed the σ the agent EXPECTED before ACT
 * and the σ the agent saw AFTER ACT (e.g. did the answer pan out?). */
void  cos_sigma_agent_reflect(cos_sigma_agent_t *a,
                              float sigma_predicted,
                              float sigma_observed);

/* Mean absolute calibration error over all completed cycles. */
float cos_sigma_agent_calibration_err(const cos_sigma_agent_t *a);

int   cos_sigma_agent_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_SIGMA_AGENT_H */
