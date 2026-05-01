/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/*
 * σ-speculative — small-model → big-model escalation policy.
 *
 * Given a running σ trace from a small model (BitNet 2B) and a single
 * escalation threshold τ_escalate, returns one of two routes:
 *
 *   LOCAL     σ_peak < τ_escalate           — keep the small-model
 *                                             output as-is.
 *   ESCALATE  σ_peak ≥ τ_escalate           — hand the prompt + the
 *                                             small-model's partial
 *                                             output to the big model
 *                                             (Claude / GPT / DeepSeek).
 *
 * "Speculative" in the arxiv 2504.12329 sense: the small model
 * speculates first, the big model corrects only when the small model
 * is uncertain.  The σ-trace is a running max over per-token σ during
 * generation — at any point σ exceeding τ_escalate is a hand-off
 * signal; the small model stops generating after the next segment
 * boundary (sentence / paragraph) and the big model continues from
 * the prefix.
 *
 * Contracts (v0):
 *   1. Route ∈ {LOCAL, ESCALATE} on every input.
 *   2. Monotone in σ_peak at fixed τ_escalate.
 *   3. NaN or negative σ_peak → ESCALATE (safe default: don't let
 *      corrupted telemetry keep the small model in the loop).
 *   4. Cost model: cos_sigma_speculative_cost_savings(n_total,
 *      n_escalated, eur_local_per_req, eur_api_per_req) returns the
 *      fractional savings in [0, 1] vs. API-only, deterministic, pure.
 *   5. A segment boundary test (end-of-sentence / end-of-paragraph)
 *      lets the small model complete its current unit before handing
 *      off so the big model never resumes mid-word.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef COS_SIGMA_SPECULATIVE_H
#define COS_SIGMA_SPECULATIVE_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    COS_SIGMA_ROUTE_LOCAL    = 0,
    COS_SIGMA_ROUTE_ESCALATE = 1,
} cos_sigma_route_t;

/* Pure two-state decision on a running σ peak.
 *
 * Returns ESCALATE on NaN or negative σ_peak (safe default). */
static inline cos_sigma_route_t
cos_sigma_speculative_route(float sigma_peak, float tau_escalate) {
    if (sigma_peak != sigma_peak)    return COS_SIGMA_ROUTE_ESCALATE;
    if (!(sigma_peak >= 0.0f))       return COS_SIGMA_ROUTE_ESCALATE;
    if (sigma_peak >= tau_escalate)  return COS_SIGMA_ROUTE_ESCALATE;
    return COS_SIGMA_ROUTE_LOCAL;
}

/* Running σ-peak update.  Use while generating token-by-token: start
 * peak = 0.0f, then peak = cos_sigma_speculative_peak_update(peak,
 * sigma_token) at each step.  Also returns ESCALATE early if peak
 * crosses τ at the current step. */
static inline float
cos_sigma_speculative_peak_update(float prev_peak, float sigma_token) {
    if (sigma_token != sigma_token) return prev_peak;
    if (sigma_token < 0.0f)         return prev_peak;
    return (sigma_token > prev_peak) ? sigma_token : prev_peak;
}

/* Segment-boundary predicate: true iff the last emitted character is
 * an end-of-sentence mark (.  !  ?) OR a newline OR a paragraph break.
 * The small model should keep generating until this returns true
 * before handing off so the big model never resumes mid-word. */
static inline bool
cos_sigma_speculative_is_segment_boundary(char c) {
    return (c == '.' || c == '!' || c == '?'
         || c == '\n' || c == '\r' || c == ';');
}

/* Fractional cost savings vs. API-only routing.
 *
 *   Let L = n_total - n_escalated (local-only requests),
 *       E = n_escalated (big-model requests).
 *   Cost(local)    = L * eur_local_per_req
 *   Cost(escalate) = E * (eur_local_per_req + eur_api_per_req)
 *   Cost(api_only) = n_total * eur_api_per_req
 *
 * Returns (Cost(api_only) - Cost(local) - Cost(escalate)) / Cost(api_only)
 * clamped to [0, 1].  Input pre: n_total > 0, n_escalated ≤ n_total,
 * both prices ≥ 0; violating the pre returns 0.0f. */
float cos_sigma_speculative_cost_savings(int n_total, int n_escalated,
                                         float eur_local_per_req,
                                         float eur_api_per_req);

/* Human-readable route label for logs. */
const char *cos_sigma_route_label(cos_sigma_route_t r);

/* Runtime exhaustive self-test.  Returns 0 on PASS. */
int cos_sigma_speculative_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_SIGMA_SPECULATIVE_H */
