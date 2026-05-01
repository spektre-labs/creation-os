/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/*
 * σ-reinforce — three-state gate policy.
 *
 * Given a σ measurement plus two thresholds τ_accept < τ_rethink,
 * returns one of three actions:
 *
 *     ACCEPT   σ < τ_accept                 — answer is reliable; return it.
 *     RETHINK  τ_accept ≤ σ < τ_rethink     — model might know; retry with
 *                                             a stronger CoT / first-principles
 *                                             prompt; remeasure σ.
 *     ABSTAIN  σ ≥ τ_rethink                — model does not know; do not
 *                                             answer.
 *
 * The RETHINK state is the missing middle between the 2-state
 * `cos_sigma_measurement_gate` (allow/abstain) in v259: it says the
 * model is *near* the knowledge boundary, where a second attempt with
 * explicit reasoning is empirically known to recover accuracy
 * (arxiv 2602.08520: entropy-aware self-correction lifts MMLU-Pro
 * 60% → 84% on base models without retraining).
 *
 * The primitive is pure: inputs are floats, output is an enum, no
 * allocation, no I/O, no global state.  Prompt rewriting is a
 * `const char *` → `const char *` lookup over a small static table
 * of canonical CoT suffixes; callers may pass their own prompts.
 *
 * Contracts (v0):
 *   1. Action is one of {ACCEPT, RETHINK, ABSTAIN} on every input.
 *   2. Monotone in σ at fixed τ_accept, τ_rethink.
 *   3. Monotone in τ_accept at fixed σ, τ_rethink (raising τ_accept
 *      can only move ACCEPT → RETHINK, never toward ABSTAIN).
 *   4. Rethink budget: cos_sigma_reinforce_max_rounds() returns a
 *      compile-time constant > 0.
 *   5. If σ is NaN, action is ABSTAIN (safe default).
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef COS_SIGMA_REINFORCE_H
#define COS_SIGMA_REINFORCE_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    COS_SIGMA_ACTION_ACCEPT  = 0,
    COS_SIGMA_ACTION_RETHINK = 1,
    COS_SIGMA_ACTION_ABSTAIN = 2,
} cos_sigma_action_t;

/* Maximum number of RETHINK rounds before the policy escalates to
 * ABSTAIN.  Hard budget: runaway rethink loops are the failure mode
 * we protect against. */
#define COS_SIGMA_REINFORCE_MAX_ROUNDS 3

static inline int cos_sigma_reinforce_max_rounds(void) {
    return COS_SIGMA_REINFORCE_MAX_ROUNDS;
}

/* Pure three-state decision.  Pre: sigma ≥ 0; on NaN or negatives
 * returns ABSTAIN (safe default).  Post: result is one of the three
 * enum values. */
static inline cos_sigma_action_t
cos_sigma_reinforce(float sigma, float tau_accept, float tau_rethink) {
    /* IEEE-754 NaN test: x != x. */
    if (sigma != sigma)              return COS_SIGMA_ACTION_ABSTAIN;
    if (!(sigma >= 0.0f))            return COS_SIGMA_ACTION_ABSTAIN;
    if (tau_accept > tau_rethink)    return COS_SIGMA_ACTION_ABSTAIN;
    if (sigma <  tau_accept)         return COS_SIGMA_ACTION_ACCEPT;
    if (sigma <  tau_rethink)        return COS_SIGMA_ACTION_RETHINK;
    return COS_SIGMA_ACTION_ABSTAIN;
}

/* Round-aware policy: after the N-th RETHINK the policy hard-falls
 * to ABSTAIN regardless of σ.  This is the "max 3 kierrosta" rule.
 * `round` is 0-indexed; round==0 is the first attempt. */
static inline cos_sigma_action_t
cos_sigma_reinforce_round(float sigma,
                          float tau_accept, float tau_rethink,
                          int round) {
    cos_sigma_action_t a = cos_sigma_reinforce(sigma, tau_accept, tau_rethink);
    if (a == COS_SIGMA_ACTION_RETHINK
     && round >= COS_SIGMA_REINFORCE_MAX_ROUNDS - 1) {
        return COS_SIGMA_ACTION_ABSTAIN;
    }
    return a;
}

/* Canonical RETHINK prompt suffix, appended to the original prompt on
 * the second attempt.  Deliberately short + language-neutral so a
 * BitNet-2B tokenizer covers it in < 20 tokens.  Callers may use
 * their own suffix; this is the reference. */
const char *cos_sigma_reinforce_rethink_suffix(void);

/* Human-readable action label ("ACCEPT" / "RETHINK" / "ABSTAIN")
 * suitable for log lines and JSON. */
const char *cos_sigma_action_label(cos_sigma_action_t a);

/* Runtime exhaustive self-test over canonical table + LCG grid + NaN
 * specials.  Returns 0 on full PASS, a positive code on failure. */
int cos_sigma_reinforce_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_SIGMA_REINFORCE_H */
