/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * v54 σ-Proconductor — subagent registry + orchestration policy.
 *
 * Honest scope: this file defines the types and pure-C policy surface
 * for multi-LLM orchestration. It does NOT make any network calls.
 * A caller passes pre-fetched responses + logits/σ from each subagent;
 * the proconductor decides who to consult (dispatch) and how to
 * combine answers (aggregate). That split keeps creation.md
 * invariant #3 (no implicit network) intact.
 *
 * Relation to v51/v53: this is the "execute" phase of the v51
 * cognitive loop, expanded from a single local forward-pass to a
 * σ-profile-driven ensemble over frontier subagents. v53's σ-TAOR
 * abstain outcomes remain the outer contract.
 */
#ifndef CREATION_OS_V54_PROCONDUCTOR_H
#define CREATION_OS_V54_PROCONDUCTOR_H

#include "../sigma/decompose.h"

#ifdef __cplusplus
extern "C" {
#endif

#define V54_MAX_AGENTS       8
#define V54_MAX_NAME         32
#define V54_MAX_DOMAIN_NAME  24

typedef enum {
    V54_DOMAIN_LOGIC    = 0, /* mathematical / formal reasoning       */
    V54_DOMAIN_FACTUAL  = 1, /* world knowledge / retrieval           */
    V54_DOMAIN_CREATIVE = 2, /* generation / style / synthesis        */
    V54_DOMAIN_CODE     = 3, /* programming / debugging               */
    V54_DOMAIN_META     = 4, /* reasoning about reasoning             */
    V54_N_DOMAINS       = 5
} v54_domain_t;

/* One frontier (or local) model registered as a subagent. No live
 * endpoint is stored — the caller decides how to dispatch. `endpoint`
 * is a free-form label (e.g. "local/bitnet-2b4t", "external/claude")
 * used only for logs and routing-policy decisions. */
typedef struct {
    char  name[V54_MAX_NAME];
    char  endpoint[V54_MAX_NAME];
    float cost_per_1k_tokens;          /* informational; policy input  */
    float latency_p50_ms;              /* informational; policy input  */

    /* σ-profile: expected per-domain σ, lower = better. Hand-tuned
     * initially, learned via v54_learn_profile_update() over time. */
    float sigma_profile[V54_N_DOMAINS];

    /* Empirical calibration signals updated by the learner. */
    float observed_accuracy[V54_N_DOMAINS]; /* in [0,1], higher = better */
    int   requests_made;
} v54_subagent_t;

typedef struct {
    v54_subagent_t agents[V54_MAX_AGENTS];
    int            n_agents;

    /* Policy knobs. */
    float convergence_threshold;   /* σ_ensemble abstain if > this   */
    float disagreement_abstain;    /* pairwise disagreement ceiling   */
    int   max_parallel_queries;    /* cost cap; 1..V54_MAX_AGENTS      */
} v54_proconductor_t;

/* ---- construction / seeding ----------------------------------- */

/* Zero-init and set sensible defaults (thresholds, caps). */
void v54_proconductor_init(v54_proconductor_t *p);

/* Register a subagent. Returns index ≥ 0 on success, −1 on overflow /
 * bad argument. σ-profile values are clipped to [0,1]. */
int v54_proconductor_register(v54_proconductor_t *p,
                              const char *name,
                              const char *endpoint,
                              float cost_per_1k_tokens,
                              float latency_p50_ms,
                              const float sigma_profile[V54_N_DOMAINS]);

/* Register five hand-tuned reference subagents:
 *   claude, gpt, gemini, deepseek, local_bitnet
 * with profiles drawn from the v54 design note. For tests only —
 * production use should feed measured profiles from the learner. */
int v54_proconductor_register_defaults(v54_proconductor_t *p);

const char *v54_domain_name(v54_domain_t d);

#ifdef __cplusplus
}
#endif

#endif /* CREATION_OS_V54_PROCONDUCTOR_H */
