/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * v54 σ-dispatch — classify, select, aggregate.
 *
 * Honest scope: "classify" is a keyword-heuristic scaffold, not a
 * local BitNet forward pass; "dispatch" is a policy that returns the
 * list of subagents to consult — the caller performs the actual
 * inference (network or local) and feeds responses back as
 * v54_response_t; "aggregate" combines responses under σ-weighted
 * voting with explicit convergence / disagreement abstain outcomes.
 *
 * This split means the test suite (deterministic, offline) can verify
 * the orchestration policy without any external API.
 */
#ifndef CREATION_OS_V54_DISPATCH_H
#define CREATION_OS_V54_DISPATCH_H

#include "proconductor.h"

#ifdef __cplusplus
extern "C" {
#endif

#define V54_MAX_RESPONSE_BYTES 512

typedef struct {
    v54_domain_t primary;
    v54_domain_t secondary;
    float complexity;  /* 0.0 simple → 1.0 research-grade    */
    float stakes;      /* 0.0 exploratory → 1.0 high-stakes  */
} v54_query_profile_t;

/* One subagent's reply. Scaffold stores the text inline so the harness
 * can compute lexical similarity without calling an embedding model. */
typedef struct {
    int   agent_index;        /* into proconductor_t.agents */
    float reported_sigma;     /* σ the agent's own decoder reported  */
    float latency_ms;
    int   cost_tokens;
    char  text[V54_MAX_RESPONSE_BYTES];
} v54_response_t;

typedef enum {
    V54_AGG_CONSENSUS        = 0, /* responses agree; return consensus   */
    V54_AGG_SIGMA_WINNER     = 1, /* σ-weighted vote picked a winner     */
    V54_AGG_ABSTAIN_SIGMA    = 2, /* σ_ensemble > convergence_threshold  */
    V54_AGG_ABSTAIN_DISAGREE = 3, /* disagreement above threshold         */
    V54_AGG_EMPTY            = 4  /* no responses                         */
} v54_agg_outcome_t;

typedef struct {
    v54_agg_outcome_t outcome;
    int               winner_index;   /* proconductor.agents[.] if ≥ 0  */
    float             sigma_ensemble; /* product of σs (or min vote σ)  */
    float             disagreement;   /* 1 − mean pairwise similarity    */
    char              answer[V54_MAX_RESPONSE_BYTES];
} v54_aggregation_t;

/* ---- Phase 1: classify -------------------------------------- */

/* Keyword-heuristic classifier. Returns primary + secondary domain
 * based on substring hits. Complexity / stakes default to 0.5 and
 * may be overridden by the caller. */
void v54_classify_query(const char *query, v54_query_profile_t *out);

/* ---- Phase 2: select ---------------------------------------- */

/* Fill `selected[0..K-1]` with agent indices into `p->agents`. Returns K.
 * Selection rule (deterministic):
 *   1. Score each agent: fit = 0.7·(1 − σ_primary) + 0.3·(1 − σ_secondary);
 *      cost_penalty = cost/100; latency_penalty = latency/10000;
 *      score = fit − cost_penalty − latency_penalty.
 *   2. K depends on stakes:
 *        stakes < 0.30  → K = 1
 *        stakes < 0.70  → K = 2
 *        stakes ≥ 0.70  → K = min(4, p->max_parallel_queries, n_agents)
 *      If any agent has σ_primary < 0.10, force K = 1 (easy query).
 *   3. Select top-K by score.
 */
int v54_select_subagents(const v54_proconductor_t *p,
                         const v54_query_profile_t *profile,
                         int *selected, int cap);

/* ---- Phase 3: dispatch (caller-driven) ---------------------- */
/* There is no v54_dispatch_call() in this scaffold: callers send
 * real API calls (respecting creation.md invariant #3) and pass the
 * resulting v54_response_t[] back into v54_aggregate_responses(). */

/* ---- Phase 4: aggregate ------------------------------------- */

/* σ-weighted aggregation with consensus / disagreement outcomes. */
void v54_aggregate_responses(const v54_proconductor_t *p,
                             const v54_response_t *responses, int n,
                             v54_aggregation_t *out);

#ifdef __cplusplus
}
#endif

#endif /* CREATION_OS_V54_DISPATCH_H */
