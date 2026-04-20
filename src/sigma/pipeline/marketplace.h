/*
 * σ-Marketplace — pay-per-query provider selection with SCSL-
 * aware reputation.
 *
 * Providers register (provider_id, model_name, price_per_query,
 * expected σ, expected latency).  The caller asks the market for
 * the cheapest provider that clears a σ and latency budget; the
 * market may also refuse every candidate and return NULL, which
 * the caller escalates to a more expensive tier or ABSTAIN.
 *
 * Tier suggestion (from SCSL cost ladder):
 *   local        0€       — own kernel
 *   neighbour    0.001€   — mesh peer (see sigma_mesh)
 *   marketplace  0.01€    — pro provider (this module)
 *   cloud API    0.10€    — frontier fallback
 *
 * Selection:
 *   candidate := { p : p.mean_sigma ≤ σ_budget
 *                   ∧ p.mean_latency ≤ latency_budget
 *                   ∧ p.price_per_query ≤ price_budget
 *                   ∧ p.scsl_accepted
 *                   ∧ p.banned == 0 }
 *   pick      := argmin_{p ∈ candidate} p.price_per_query
 *   tiebreak  := argmin p.mean_sigma, then argmin p.mean_latency
 *
 * Reputation (post-query):
 *   σ_provider ← α σ_provider + (1−α) σ_result
 *   latency    ← α latency    + (1−α) latency_observed
 *   success    → n_successes++
 *   failure    → n_failures++
 *   If n_failures grows fast relative to n_successes *and* σ
 *   stays above σ_ban_floor, the provider is banned (SCSL
 *   violation signal — banned is sticky until unban()).
 *
 * SCSL compliance:
 *   A provider must set scsl_accepted = 1 on registration.
 *   The market will never route to a provider with
 *   scsl_accepted == 0.  report_scsl_violation(id) flips the
 *   ban flag and σ is forced to 1.0 — the lease-style license
 *   (SCSL) is the protocol, not just prose.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef COS_SIGMA_MARKETPLACE_H
#define COS_SIGMA_MARKETPLACE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_MKT_ID_CAP    32
#define COS_MKT_MODEL_CAP 32

typedef struct {
    char   provider_id[COS_MKT_ID_CAP];
    char   model_name[COS_MKT_MODEL_CAP];
    double price_per_query;   /* EUR (so we can sum 1k queries)  */
    float  mean_sigma;         /* EWMA reputation (0..1)          */
    float  mean_latency_ms;
    int    n_successes;
    int    n_failures;
    double total_spent_eur;
    int    scsl_accepted;      /* 1 = license terms acknowledged  */
    int    banned;             /* 1 = SCSL violation, never route */
} cos_mkt_provider_t;

typedef struct {
    cos_mkt_provider_t *slots;
    int                 cap;
    int                 count;
    float               ewma_alpha;
    float               sigma_ban_floor; /* σ ≥ floor AND failure
                                          * rate high → auto ban  */
    double              total_queries;
    double              total_spend_eur;
} cos_mkt_t;

int   cos_sigma_mkt_init(cos_mkt_t *mkt,
                         cos_mkt_provider_t *storage, int cap,
                         float ewma_alpha,
                         float sigma_ban_floor);

int   cos_sigma_mkt_register(cos_mkt_t *mkt,
                             const char *provider_id,
                             const char *model_name,
                             double price_per_query,
                             float  mean_sigma,
                             float  mean_latency_ms,
                             int    scsl_accepted);

/* Ban / unban ops.  Banning forces σ → 1.0 so this provider is
 * also filtered out of routing if someone relaxes the banned
 * flag check downstream. */
int   cos_sigma_mkt_ban  (cos_mkt_t *mkt, const char *provider_id,
                          const char *reason);
int   cos_sigma_mkt_unban(cos_mkt_t *mkt, const char *provider_id);

const cos_mkt_provider_t *cos_sigma_mkt_select(
    const cos_mkt_t *mkt,
    double price_budget_eur,
    float  sigma_budget,
    float  latency_budget_ms);

/* Feedback: σ_result is the downstream gate's σ stamped on the
 * provider's answer; latency_observed is the observed RTT; cost
 * is the billed amount (may differ from list price with volume
 * discounts). */
int   cos_sigma_mkt_report(cos_mkt_t *mkt,
                           const char *provider_id,
                           float  sigma_result,
                           float  latency_observed_ms,
                           double cost_eur,
                           int    success);

int   cos_sigma_mkt_find(const cos_mkt_t *mkt,
                         const char *provider_id, int *out_idx);

int   cos_sigma_mkt_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_SIGMA_MARKETPLACE_H */
